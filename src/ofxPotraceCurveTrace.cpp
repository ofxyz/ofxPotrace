#include "ofxPotraceCurveTrace.h"

#include "ofLog.h"

extern "C" {
#include "potracelib.h"
#include "bitmap.h"
}

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <memory>

namespace ofxPotrace {

namespace {

struct BitmapDeleter {
    void operator()(potrace_bitmap_t* bm) const { bm_free(bm); }
};

struct ParamDeleter {
    void operator()(potrace_param_t* p) const { potrace_param_free(p); }
};

struct StateDeleter {
    void operator()(potrace_state_t* st) const { potrace_state_free(st); }
};

glm::dvec2 cubicAt(double t, const potrace_dpoint_t& p0, const potrace_dpoint_t& c1,
                   const potrace_dpoint_t& c2, const potrace_dpoint_t& p1) {
    const double u = 1.0 - t;
    const double uu = u * u;
    const double tt = t * t;
    const double uuu = uu * u;
    const double ttt = tt * t;
    return {
        uuu * p0.x + 3.0 * uu * t * c1.x + 3.0 * u * tt * c2.x + ttt * p1.x,
        uuu * p0.y + 3.0 * uu * t * c1.y + 3.0 * u * tt * c2.y + ttt * p1.y
    };
}

void appendCurve(const potrace_curve_t& curve, int resolution, ofPolyline& out) {
    if (curve.n <= 0 || resolution < 1) return;

    potrace_dpoint_t start = curve.c[curve.n - 1][2];
    out.addVertex(static_cast<float>(start.x), static_cast<float>(start.y));

    for (int i = 0; i < curve.n; ++i) {
        if (curve.tag[i] == POTRACE_CURVETO) {
            const potrace_dpoint_t c1 = curve.c[i][0];
            const potrace_dpoint_t c2 = curve.c[i][1];
            const potrace_dpoint_t end = curve.c[i][2];
            for (int s = 1; s <= resolution; ++s) {
                const double t = static_cast<double>(s) / static_cast<double>(resolution);
                const glm::dvec2 p = cubicAt(t, start, c1, c2, end);
                out.addVertex(static_cast<float>(p.x), static_cast<float>(p.y));
            }
            start = end;
        } else {
            const potrace_dpoint_t corner = curve.c[i][1];
            out.addVertex(static_cast<float>(corner.x), static_cast<float>(corner.y));
            start = corner;
        }
    }
    out.setClosed(true);
}

void collectPaths(potrace_path_t* plist, bool traceHoles, int resolution,
                  std::vector<ofPolyline>& out) {
    for (potrace_path_t* p = plist; p != nullptr; p = p->next) {
        if (!traceHoles && p->sign == '-') continue;
        if (p->curve.n <= 0) continue;

        ofPolyline pl;
        appendCurve(p->curve, resolution, pl);
        if (pl.size() >= 3) {
            out.push_back(std::move(pl));
        }
    }
}

std::unique_ptr<potrace_bitmap_t, BitmapDeleter> greyToBitmap(
    const unsigned char* grey, int w, int h,
    int thresholdMin, int thresholdMax, bool invert) {
    std::unique_ptr<potrace_bitmap_t, BitmapDeleter> bm(bm_new(w, h));
    if (!bm) return nullptr;

    int tMin = std::clamp(thresholdMin, 0, 255);
    int tMax = std::clamp(thresholdMax, 0, 255);
    if (tMin > tMax) std::swap(tMin, tMax);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char g = grey[y * w + x];
            bool ink = (g >= tMin && g <= tMax);
            if (invert) ink = !ink;
            BM_PUT(bm.get(), x, y, ink ? 1 : 0);
        }
    }
    return bm;
}

unsigned char sampleGreyBilinear(const unsigned char* grey, int w, int h, float x, float y) {
    x = std::clamp(x, 0.f, static_cast<float>(w - 1));
    y = std::clamp(y, 0.f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float v00 = grey[y0 * w + x0];
    const float v10 = grey[y0 * w + x1];
    const float v01 = grey[y1 * w + x0];
    const float v11 = grey[y1 * w + x1];
    const float v0 = v00 + (v10 - v00) * tx;
    const float v1 = v01 + (v11 - v01) * tx;
    return static_cast<unsigned char>(std::round(v0 + (v1 - v0) * ty));
}

std::vector<unsigned char> upscaleGrey(
    const unsigned char* grey, int w, int h, float scale, int& outW, int& outH) {
    outW = std::max(1, static_cast<int>(std::round(w * scale)));
    outH = std::max(1, static_cast<int>(std::round(h * scale)));
    std::vector<unsigned char> up(static_cast<std::size_t>(outW * outH));
    const float invScale = 1.f / scale;
    for(int y = 0; y < outH; ++y) {
        for(int x = 0; x < outW; ++x) {
            const float srcX = (static_cast<float>(x) + 0.5f) * invScale - 0.5f;
            const float srcY = (static_cast<float>(y) + 0.5f) * invScale - 0.5f;
            up[static_cast<std::size_t>(y * outW + x)] =
                sampleGreyBilinear(grey, w, h, srcX, srcY);
        }
    }
    return up;
}

void scalePolylinesToImage(std::vector<ofPolyline>& paths, float invTraceScale) {
    if(std::abs(invTraceScale - 1.f) < 1e-6f) {
        return;
    }
    for(auto& pl : paths) {
        auto& verts = pl.getVertices();
        for(auto& v : verts) {
            v *= invTraceScale;
        }
    }
}

std::vector<ofPolyline> traceBitmap(potrace_bitmap_t* bm, const CurveTraceSettings& settings) {
    std::vector<ofPolyline> result;
    if (!bm) return result;

    std::unique_ptr<potrace_param_t, ParamDeleter> param(potrace_param_default());
    if (!param) {
        ofLogError("ofxPotrace::CurveTrace") << "potrace_param_default failed";
        return result;
    }

    param->turdsize = std::max(0, settings.turdsize);
    param->turnpolicy = POTRACE_TURNPOLICY_MINORITY;
    param->alphamax = std::clamp(static_cast<double>(settings.alphamax), 0.0, 4.0 / 3.0);
    param->opticurve = settings.opticurve ? 1 : 0;
    param->opttolerance = std::max(0.0, static_cast<double>(settings.opttolerance));

    std::unique_ptr<potrace_state_t, StateDeleter> state(potrace_trace(param.get(), bm));
    if (!state || !state->plist) {
        ofLogWarning("ofxPotrace::CurveTrace") << "potrace_trace produced no paths";
        return result;
    }

    if (state->status == POTRACE_STATUS_INCOMPLETE) {
        ofLogWarning("ofxPotrace::CurveTrace") << "potrace_trace incomplete";
    }

    const int resolution = std::max(2, settings.curveResolution);
    collectPaths(state->plist, settings.traceHoles, resolution, result);
    return result;
}

} // namespace

bool CurveTrace::isLibraryAvailable() {
    return potrace_version() != nullptr;
}

const char* CurveTrace::libraryVersion() {
    return potrace_version();
}

std::vector<ofPolyline> CurveTrace::traceGreyscale(
    const unsigned char* grey,
    int w,
    int h,
    int thresholdMin,
    int thresholdMax,
    bool invert,
    const CurveTraceSettings& settings) {
    const float traceScale = std::clamp(settings.traceScale, 1.0f, 8.0f);
    if(traceScale > 1.001f) {
        int tw = w;
        int th = h;
        std::vector<unsigned char> upscaled = upscaleGrey(grey, w, h, traceScale, tw, th);
        auto bm = greyToBitmap(upscaled.data(), tw, th, thresholdMin, thresholdMax, invert);
        if(!bm) {
            return {};
        }
        auto paths = traceBitmap(bm.get(), settings);
        scalePolylinesToImage(paths, 1.f / traceScale);
        return paths;
    }

    auto bm = greyToBitmap(grey, w, h, thresholdMin, thresholdMax, invert);
    if (!bm) return {};
    return traceBitmap(bm.get(), settings);
}

std::vector<ofPolyline> CurveTrace::traceMask(
    const ofPixels& mask,
    const CurveTraceSettings& settings) {
    if (!mask.isAllocated() || mask.getWidth() <= 0 || mask.getHeight() <= 0) {
        return {};
    }

    const int w = mask.getWidth();
    const int h = mask.getHeight();

    std::unique_ptr<potrace_bitmap_t, BitmapDeleter> bm(bm_new(w, h));
    if (!bm) return {};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const bool ink = mask.getColor(x, y).getBrightness() > 127;
            BM_PUT(bm.get(), x, y, ink ? 1 : 0);
        }
    }
    return traceBitmap(bm.get(), settings);
}

} // namespace ofxPotrace
