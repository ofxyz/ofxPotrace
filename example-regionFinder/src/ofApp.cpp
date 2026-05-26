#include "ofApp.h"

#include "imgui_internal.h"

#include "ImFonts.h"
#include "ImThemeRegistry.h"
#include "inspectors/trace_inspectors.h"
#include <ofxPotraceCurveTrace.h>

#include <algorithm>
#include <fstream>

namespace {

ImVec4 accentColor(0.24f, 0.65f, 0.88f, 1.0f);
ImVec4 mutedColor(0.55f, 0.55f, 0.58f, 1.0f);

constexpr ImVec2 kImgUv0(0.f, 0.f);
constexpr ImVec2 kImgUv1(1.f, 1.f);

unsigned char greyFromColor(const ofColor & c) {
	return static_cast<unsigned char>(0.299f * c.r + 0.587f * c.g + 0.114f * c.b);
}

} // namespace

void ofApp::setup() {
	ofSetWindowTitle("ofxPotrace - Curve Trace");
	ofSetFrameRate(60);
	ofBackground(22, 22, 28);

	ofDisableArbTex();

	gui.setup();
	setupImGui();

	if(!ofxPotrace::CurveTrace::isLibraryAvailable()) {
		status = "libpotrace not available.";
	} else {
		status = std::string("Open an image - Potrace ") + ofxPotrace::CurveTrace::libraryVersion();
	}
}

void ofApp::setupImGui() {
	ImGuiIO & io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	imguiIniPath = ofToDataPath("imgui_potrace_curve_trace.ini", true);
	io.IniFilename = imguiIniPath.c_str();

	if(ImFont * font = ImFonts::LoadDefaultFonts(io.Fonts, 15.0f)) {
		gui.setDefaultFont(font);
	}
	gui.rebuildFontsTexture();

	ImTheme::Setup("DarculaDarker");
}

void ofApp::update() {
}

void ofApp::draw() {
	gui.begin();
	drawDockSpace();
	drawCanvas();
	drawControls();
	drawDebug();
	drawDialogs();
	gui.end();
}

void ofApp::keyPressed(int key) {
	if(key == 'o' || key == 'O') {
		openImageDialog();
	}
	if(key == 'r' || key == 'R') {
		traceImage();
	}
}

void ofApp::dragEvent(ofDragInfo dragInfo) {
	if(!dragInfo.files.empty()) {
		loadImage(dragInfo.files.front().string());
	}
}

void ofApp::drawDockSpace() {
	ImGuiViewport * viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags hostFlags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("##ofxPotraceDockSpace", nullptr, hostFlags);
	ImGui::PopStyleVar();

	if(ImGui::BeginMenuBar()) {
		if(ImGui::BeginMenu("File")) {
			if(ImGui::MenuItem("Open Image...", "O")) {
				openImageDialog();
			}
			if(ImGui::MenuItem("Retrace", "R", false, hasImage)) {
				traceImage();
			}
			if(ImGui::MenuItem("Export SVG...", nullptr, false, hasImage && !paths.empty())) {
				openExportSvgDialog();
			}
			ImGui::Separator();
			if(ImGui::MenuItem("Exit")) {
				ofExit();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGuiID dockId = ImGui::GetID("##ofxPotraceDock");
	ImGui::DockSpace(dockId, ImVec2(0, 0));
	if(!dockLayoutBuilt) {
		dockLayoutBuilt = true;
		if(!ofFile::doesFileExist(imguiIniPath, false)) {
			setupDefaultDockLayout(dockId);
		}
	}
	ImGui::End();
}

void ofApp::setupDefaultDockLayout(ImGuiID dockSpaceId) {
	ImGui::DockBuilderRemoveNode(dockSpaceId);
	ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockSpaceId, ImGui::GetMainViewport()->WorkSize);

	ImGuiID dockMain = dockSpaceId;
	ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.36f, nullptr, &dockMain);
	ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.22f, nullptr, &dockMain);

	ImGui::DockBuilderDockWindow("Canvas", dockMain);
	ImGui::DockBuilderDockWindow("Potrace", dockLeft);
	ImGui::DockBuilderDockWindow("Debug", dockBottom);
	ImGui::DockBuilderFinish(dockSpaceId);
}

void ofApp::resetCanvasView() {
	canvasZoom = 1.0f;
	canvasPan = glm::vec2(0.f);
}

void ofApp::handleCanvasNavigation(const ImVec2 & /*viewportSize*/) {
	ImGuiIO & io = ImGui::GetIO();

	if(!ImGui::IsItemHovered()) {
		return;
	}

	if(io.MouseWheel != 0.f) {
		const ImVec2 mouse = ImGui::GetMousePos();
		const ImVec2 rectMin = ImGui::GetItemRectMin();
		const ImVec2 anchor(mouse.x - rectMin.x, mouse.y - rectMin.y);

		const float oldZoom = canvasZoom;
		canvasZoom *= std::pow(1.12f, io.MouseWheel);
		canvasZoom = std::clamp(canvasZoom, 0.05f, 64.f);

		const float zoomRatio = canvasZoom / oldZoom;
		canvasPan.x = anchor.x - (anchor.x - canvasPan.x) * zoomRatio;
		canvasPan.y = anchor.y - (anchor.y - canvasPan.y) * zoomRatio;
	}

	const bool panDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f) ||
		(ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f) && io.KeyShift);
	if(panDrag) {
		canvasPan.x += io.MouseDelta.x;
		canvasPan.y += io.MouseDelta.y;
	}

	if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		resetCanvasView();
	}
}

void ofApp::drawCanvas() {
	ImGui::SetNextWindowSize(ImVec2(760, 620), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
	ImGui::Begin("Canvas");

	if(!hasImage) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPos(ImVec2(avail.x * 0.5f - 150.0f, avail.y * 0.5f - 10.0f));
		ImGui::TextColored(mutedColor, "Open or drop an image to trace with Potrace.");
		ImGui::End();
		return;
	}

	const float imageW = image.getWidth();
	const float imageH = image.getHeight();

	ImGui::TextColored(mutedColor, "Wheel: zoom  |  Shift+drag / MMB: pan  |  Dbl-click: reset");
	ImGui::SameLine();
	ImGui::Text("|  %.0f%%", canvasZoom * 100.f);

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	viewportSize.y = std::max(viewportSize.y, 32.f);

	ImGui::InvisibleButton("##canvasViewport", viewportSize);
	handleCanvasNavigation(viewportSize);

	const ImVec2 viewportMin = ImGui::GetItemRectMin();
	const float fitScale = std::max(0.001f, std::min(viewportSize.x / imageW, viewportSize.y / imageH));
	const float scale = fitScale * canvasZoom;
	const ImVec2 drawSize(imageW * scale, imageH * scale);
	const ImVec2 imageMin(
		viewportMin.x + (viewportSize.x - drawSize.x) * 0.5f + canvasPan.x,
		viewportMin.y + (viewportSize.y - drawSize.y) * 0.5f + canvasPan.y);

	const ImVec2 viewportMax(viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y);
	ImDrawList * drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(viewportMin, viewportMax, true);

	const bool canShowImage = showImage && image.isAllocated() && image.getTexture().isAllocated();
	if(canShowImage) {
		drawList->AddImage(
			GetImTextureID(image.getTexture()),
			imageMin,
			ImVec2(imageMin.x + drawSize.x, imageMin.y + drawSize.y),
			kImgUv0,
			kImgUv1);
	}

	if(showMask) {
		drawMaskPreview(imageMin, drawSize);
	}

	if(showPaths) {
		drawPathsOverlay(drawList, imageMin, scale);
	}

	drawList->PopClipRect();
	ImGui::End();
}

void ofApp::drawControls() {
	ImGui::SetNextWindowSize(ImVec2(520, 680), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(800, 40), ImGuiCond_FirstUseEver);
	ImGui::Begin("Potrace");

	const float buttonH = ImGui::GetFrameHeightWithSpacing() + 10.f;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 8.f));

	if(ImGui::Button("Open Image...", ImVec2(-1.f, buttonH))) {
		openImageDialog();
	}

	ImGui::PopStyleVar();
	ImGui::Spacing();
	ImGui::TextColored(accentColor, "%s", status.c_str());
	if(!imagePath.empty()) {
		ImGui::TextWrapped("%s", imagePath.c_str());
	}

	ImGui::Separator();
	ImGui::TextDisabled("Threshold");
	inspector::ComponentInspector thresholdPanel("Threshold");
	inspector::registerProperties(threshold, thresholdPanel);
	bool changed = thresholdPanel.drawPanel();

	ImGui::Spacing();
	ImGui::TextDisabled("Curve fit");
	inspector::ComponentInspector curvePanel("Curve");
	inspector::registerProperties(curve, curvePanel);
	changed |= curvePanel.drawPanel();

	if(ImGui::IsAnyItemActive()) {
		changed = true;
	}

	ImGui::Separator();
	ImGui::Checkbox("Show Image", &showImage);
	ImGui::Checkbox("Show Threshold Mask", &showMask);
	ImGui::Checkbox("Show Paths", &showPaths);
	ImGui::Checkbox("Fill Paths", &fillPaths);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 8.f));
	if(ImGui::Button("Trace Now", ImVec2(-1.f, buttonH))) {
		traceImage();
	}

	ImGui::BeginDisabled(paths.empty());
	if(ImGui::Button("Export SVG...", ImVec2(-1.f, buttonH))) {
		openExportSvgDialog();
	}
	ImGui::EndDisabled();
	ImGui::PopStyleVar();

	if(changed && hasImage) {
		traceImage();
	}

	ImGui::End();
}

void ofApp::drawDebug() {
	ImGui::SetNextWindowSize(ImVec2(360, 200), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(800, 680), ImGuiCond_FirstUseEver);
	ImGui::Begin("Debug");

	ImGui::Text("Image: %.0f x %.0f", image.getWidth(), image.getHeight());
	if(hasImage) {
		const float ts = std::clamp(curve.traceScale, 1.f, 8.f);
		ImGui::Text(
			"Trace bitmap: %.0f x %.0f (%.2fx)",
			image.getWidth() * ts,
			image.getHeight() * ts,
			ts);
	}
	ImGui::Text("Paths: %zu", paths.size());
	std::size_t points = 0;
	for(const auto & pl: paths) {
		points += pl.size();
	}
	ImGui::Text("Vertices: %zu", points);
	ImGui::Text("Last trace: %.2f ms", lastTraceMs);
	ImGui::Text("FPS: %.1f", ofGetFrameRate());
	ImGui::Text("Library: %s", ofxPotrace::CurveTrace::libraryVersion());

	ImGui::End();
}

void ofApp::drawDialogs() {
	ImVec2 minSize(640, 420);
	ImVec2 maxSize(1200, 800);
	if(ImGuiFileDialog::Instance()->Display("OpenImageDlg", ImGuiWindowFlags_NoCollapse, minSize, maxSize)) {
		if(ImGuiFileDialog::Instance()->IsOk()) {
			loadImage(ImGuiFileDialog::Instance()->GetFilePathName());
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if(ImGuiFileDialog::Instance()->Display("ExportSvgDlg", ImGuiWindowFlags_NoCollapse, minSize, maxSize)) {
		if(ImGuiFileDialog::Instance()->IsOk()) {
			exportPathsToSvg(ImGuiFileDialog::Instance()->GetFilePathName());
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void ofApp::drawMaskPreview(const ImVec2 & imageMin, const ImVec2 & imageSize) {
	if(!hasMaskTexture) {
		return;
	}

	ImGui::GetWindowDrawList()->AddImage(
		GetImTextureID(maskTexture),
		imageMin,
		ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y),
		kImgUv0,
		kImgUv1,
		IM_COL32(255, 255, 255, 120));
}

void ofApp::drawPathsOverlay(ImDrawList * drawList, const ImVec2 & imageMin, float scale) {
	if(!drawList) {
		return;
	}
	for(const auto & pl: paths) {
		const auto & vertices = pl.getVertices();
		if(vertices.size() < 2) {
			continue;
		}

		const ImU32 stroke = IM_COL32(80, 240, 120, 240);
		if(fillPaths && pl.isClosed()) {
			std::vector<ImVec2> points;
			points.reserve(vertices.size());
			for(const auto & v: vertices) {
				points.emplace_back(imageMin.x + v.x * scale, imageMin.y + v.y * scale);
			}
			// Potrace paths are concave — AddConvexPolyFilled fans to vertex 0 (spikes).
			drawList->AddConcavePolyFilled(points.data(), static_cast<int>(points.size()), IM_COL32(80, 240, 120, 40));
		}

		for(std::size_t i = 1; i < vertices.size(); ++i) {
			const auto & a = vertices[i - 1];
			const auto & b = vertices[i];
			drawList->AddLine(
				ImVec2(imageMin.x + a.x * scale, imageMin.y + a.y * scale),
				ImVec2(imageMin.x + b.x * scale, imageMin.y + b.y * scale),
				stroke,
				2.0f);
		}
		if(pl.isClosed() && vertices.size() > 1) {
			const auto & a = vertices.back();
			const auto & b = vertices.front();
			drawList->AddLine(
				ImVec2(imageMin.x + a.x * scale, imageMin.y + a.y * scale),
				ImVec2(imageMin.x + b.x * scale, imageMin.y + b.y * scale),
				stroke,
				2.0f);
		}
	}
}

void ofApp::openImageDialog() {
	IGFD::FileDialogConfig config;
	config.path = ofToDataPath("", true);
	ImGuiFileDialog::Instance()->OpenDialog(
		"OpenImageDlg",
		"Open Image",
		"Images{.png,.jpg,.jpeg,.bmp,.gif,.tga},All files{.*}",
		config);
}

void ofApp::loadImage(const std::string & path) {
	if(!isImagePath(path)) {
		status = "Unsupported image type.";
		return;
	}

	image.clear();
	if(!image.load(path)) {
		status = "Failed to load image.";
		hasImage = false;
		return;
	}

	image.setImageType(OF_IMAGE_COLOR);
	image.update();
	hasImage = image.isAllocated() && image.getWidth() > 0 && image.getHeight() > 0;
	if(!hasImage) {
		status = "Failed to prepare image texture.";
		return;
	}

	imagePath = path;
	resetCanvasView();
	buildGreyscalePixels();
	traceImage();
	status = "Loaded: " + ofFilePath::getFileName(path);
}

void ofApp::buildGreyscalePixels() {
	const int w = image.getWidth();
	const int h = image.getHeight();
	greyPixels.allocate(w, h, OF_PIXELS_GRAY);
	const auto & src = image.getPixels();
	for(int y = 0; y < h; ++y) {
		for(int x = 0; x < w; ++x) {
			greyPixels[x + y * w] = greyFromColor(src.getColor(x, y));
		}
	}
}

void ofApp::traceImage() {
	if(!hasImage || !greyPixels.isAllocated()) {
		return;
	}

	const int w = greyPixels.getWidth();
	const int h = greyPixels.getHeight();

	uint64_t start = ofGetElapsedTimeMicros();
	paths = ofxPotrace::CurveTrace::traceGreyscale(
		greyPixels.getData(),
		w,
		h,
		threshold.valueMin,
		threshold.valueMax,
		threshold.invert,
		curve);
	lastTraceMs = static_cast<float>(ofGetElapsedTimeMicros() - start) / 1000.0f;

	updateThresholdMaskTexture();

	if(paths.empty()) {
		status = "Trace: 0 paths — adjust threshold (Show Threshold Mask).";
	} else {
		status = "Trace: " + std::to_string(paths.size()) + " path(s) - smooth Potrace outlines.";
	}
}

void ofApp::updateThresholdMaskTexture() {
	if(!greyPixels.isAllocated()) {
		hasMaskTexture = false;
		return;
	}

	const int w = greyPixels.getWidth();
	const int h = greyPixels.getHeight();
	int tMin = std::clamp(threshold.valueMin, 0, 255);
	int tMax = std::clamp(threshold.valueMax, 0, 255);
	if(tMin > tMax) {
		std::swap(tMin, tMax);
	}

	ofPixels mask;
	mask.allocate(w, h, OF_PIXELS_GRAY);
	for(int i = 0; i < w * h; ++i) {
		const unsigned char g = greyPixels[i];
		bool ink = (g >= tMin && g <= tMax);
		if(threshold.invert) {
			ink = !ink;
		}
		mask[i] = ink ? 255 : 0;
	}

	maskTexture.loadData(mask);
	hasMaskTexture = maskTexture.isAllocated();
}

void ofApp::openExportSvgDialog() {
	if(paths.empty()) {
		return;
	}
	IGFD::FileDialogConfig config;
	config.path = imagePath.empty() ? ofToDataPath("", true) : ofFilePath::getEnclosingDirectory(imagePath);
	ImGuiFileDialog::Instance()->OpenDialog(
		"ExportSvgDlg",
		"Export SVG",
		"SVG{.svg},All files{.*}",
		config);
}

void ofApp::exportPathsToSvg(const std::string & path) {
	const int w = image.getWidth();
	const int h = image.getHeight();
	if(w <= 0 || h <= 0) {
		status = "Export failed: no image size.";
		return;
	}

	std::string outPath = path;
	if(ofToLower(ofFilePath::getFileExt(outPath)) != "svg") {
		outPath += ".svg";
	}

	std::ofstream out(ofPathToString(of::filesystem::path(outPath)));
	if(!out) {
		status = "Export failed: could not write file.";
		return;
	}

	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w << "\" height=\"" << h
		<< "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
	out << "<g fill=\"none\" stroke=\"#00ff88\" stroke-width=\"1.5\">\n";

	for(const auto & pl: paths) {
		const auto & verts = pl.getVertices();
		if(verts.size() < 2) {
			continue;
		}
		out << "<path d=\"M " << verts[0].x << " " << verts[0].y;
		for(std::size_t i = 1; i < verts.size(); ++i) {
			out << " L " << verts[i].x << " " << verts[i].y;
		}
		if(pl.isClosed()) {
			out << " Z";
		}
		out << "\"/>\n";
	}

	out << "</g>\n</svg>\n";
	out.close();

	status = "Exported " + std::to_string(paths.size()) + " path(s) to " + ofFilePath::getFileName(outPath);
}

bool ofApp::isImagePath(const std::string & path) const {
	std::string ext = ofToLower(ofFilePath::getFileExt(path));
	return ext == "png" || ext == "jpg" || ext == "jpeg" ||
		   ext == "bmp" || ext == "gif" || ext == "tga";
}
