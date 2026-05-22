#pragma once

#include "ofPolyline.h"
#include "ofPixels.h"

#include "components/trace_components.h"

#include <vector>

namespace ofxPotrace {

using CurveTraceSettings = ecs::curve_trace_settings;

// Trace a binary mask with libpotrace and return smooth outline polylines (pixel coords).
// Set bits (non-zero mask pixels) become the filled regions Potrace vectorizes.

class CurveTrace {

public:

    /// Greyscale 0–255; pixels in [thresholdMin, thresholdMax] become ink (unless invert).
    static std::vector<ofPolyline> traceGreyscale(
        const unsigned char* grey,
        int w,
        int h,
        int thresholdMin,
        int thresholdMax,
        bool invert,
        const CurveTraceSettings& settings = {});

    static std::vector<ofPolyline> traceMask(
        const ofPixels& mask,
        const CurveTraceSettings& settings = {});

    static bool isLibraryAvailable();
    static const char* libraryVersion();

};

} // namespace ofxPotrace
