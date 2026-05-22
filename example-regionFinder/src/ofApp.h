#pragma once

#include "ofMain.h"
#include "ofxImGui.h"
#include "ofxImGuiFileDialog.h"

#include "components/trace_components.h"

#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void dragEvent(ofDragInfo dragInfo) override;

private:
	void setupImGui();
	void drawDockSpace();
	void setupDefaultDockLayout(ImGuiID dockSpaceId);
	void drawCanvas();
	void drawControls();
	void drawDebug();
	void drawDialogs();
	void drawMaskPreview(const ImVec2 & imageMin, const ImVec2 & imageSize);
	void drawPathsOverlay(ImDrawList * drawList, const ImVec2 & imageMin, float scale);
	void resetCanvasView();
	void handleCanvasNavigation(const ImVec2 & viewportSize);

	void openImageDialog();
	void loadImage(const std::string & path);
	void traceImage();
	void buildGreyscalePixels();
	void updateThresholdMaskTexture();
	void openExportSvgDialog();
	void exportPathsToSvg(const std::string & path);
	bool isImagePath(const std::string & path) const;

	ofxImGui::Gui gui;
	ofImage image;
	ofPixels greyPixels;
	ofTexture maskTexture;
	bool hasImage = false;
	bool hasMaskTexture = false;

	ecs::greyscale_threshold_settings threshold;
	ecs::curve_trace_settings curve;

	std::vector<ofPolyline> paths;

	std::string imagePath;
	std::string imguiIniPath;
	std::string status = "Open an image to trace with Potrace.";
	bool dockLayoutBuilt = false;
	float lastTraceMs = 0.0f;

	bool showImage = true;
	bool showMask = true;
	bool showPaths = true;
	bool fillPaths = false;

	float canvasZoom = 1.0f;
	glm::vec2 canvasPan {0.f, 0.f};
};
