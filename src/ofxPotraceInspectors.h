#pragma once

#include "ofxPotraceRegionFinder.h"
#include "ComponentInspector.h"
#include "inspectors/color_band_inspectors.h"

namespace inspector {

template<>
inline void registerProperties(ofxPotrace::RegionFinderSettings& s, ComponentInspector& inspector) {
	registerProperties(s.colorBand, inspector);

	inspector.addCustomProperty("Morphology", [&s]() {
		if(ImGui::CollapsingHeader("Morphology", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderInt("Blur", &s.blurAmount, 0, 16);
			ImGui::SliderInt("Open Iterations", &s.openIterations, 0, 8);
			ImGui::SliderInt("Close Iterations", &s.closeIterations, 0, 8);
			ImGui::Checkbox("Invert Mask", &s.invert);
			ImGui::Checkbox("Find Holes", &s.findHoles);
		}
	});

	inspector.addCustomProperty("Contour filter", [&s]() {
		if(ImGui::CollapsingHeader("Contour Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Min Area", &s.minArea, 1.0f, 0.0f, 1000000.0f, "%.0f");
			ImGui::DragFloat("Max Area", &s.maxArea, 1.0f, 0.0f, 10000000.0f, "%.0f");
			ImGui::SliderInt("Max Contours", &s.maxContours, 1, 512);
			ImGui::DragFloat("Simplify", &s.simplifyTolerance, 0.1f, 0.0f, 64.0f, "%.1f");
		}
	});
}

} // namespace inspector
