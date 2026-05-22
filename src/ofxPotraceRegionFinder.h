#pragma once

#include "ofPixels.h"
#include "ofPolyline.h"
#include "ofRectangle.h"

#include "components/color_band_component.h"

#include <vector>

namespace ofxPotrace {

using ColorBandModel = ecs::ColorBandModel;

struct RegionFinderSettings {
	ecs::color_band_component colorBand;

	int blurAmount = 0;
	int openIterations = 0;
	int closeIterations = 1;
	bool invert = false;
	bool findHoles = false;

	float minArea = 16.0f;
	float maxArea = 0.0f;
	int maxContours = 128;
	float simplifyTolerance = 0.0f;
};

struct RegionFinderStats {
	int maskForegroundPixels = 0;
	int rawContours = 0;
	int acceptedRegions = 0;
};

struct Region {
	ofPolyline contour;
	ofRectangle bounds;
	float area = 0.0f;
	bool hole = false;
};

class RegionFinder {
public:
	void setImage(const ofPixels & pixels);
	void clearImage();
	bool hasImage() const;

	void setSettings(const RegionFinderSettings & settings);
	RegionFinderSettings & getSettings();
	const RegionFinderSettings & getSettings() const;

	bool update();

	const std::vector<Region> & getRegions() const;
	std::vector<ofPolyline> getContours() const;
	const ofPixels & getMaskPixels() const;

	int getWidth() const;
	int getHeight() const;
	const RegionFinderStats & getLastStats() const;

private:
	ofPixels sourcePixels;
	ofPixels maskPixels;
	RegionFinderSettings settings;
	std::vector<Region> regions;
	RegionFinderStats lastStats;
};

} // namespace ofxPotrace
