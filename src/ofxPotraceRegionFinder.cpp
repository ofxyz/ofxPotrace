#include "ofxPotraceRegionFinder.h"

#include "ofColor.h"
#include "opencv2/imgproc.hpp"

#include <algorithm>

namespace {

int clampInt(int value, int minValue, int maxValue) {
	return std::max(minValue, std::min(maxValue, value));
}

cv::Mat pixelsToRgbMat(const ofPixels & pixels) {
	cv::Mat rgb(pixels.getHeight(), pixels.getWidth(), CV_8UC3);
	for(int y = 0; y < static_cast<int>(pixels.getHeight()); ++y) {
		for(int x = 0; x < static_cast<int>(pixels.getWidth()); ++x) {
			const ofColor color = pixels.getColor(x, y);
			rgb.at<cv::Vec3b>(y, x) = cv::Vec3b(color.r, color.g, color.b);
		}
	}
	return rgb;
}

void matToMaskPixels(const cv::Mat & mask, ofPixels & pixels) {
	pixels.allocate(mask.cols, mask.rows, OF_PIXELS_GRAY);
	for(int y = 0; y < mask.rows; ++y) {
		const unsigned char * src = mask.ptr<unsigned char>(y);
		unsigned char * dst = pixels.getData() + y * mask.cols;
		std::copy(src, src + mask.cols, dst);
	}
}

ofPolyline contourToPolyline(const std::vector<cv::Point> & contour) {
	ofPolyline polyline;
	for(const auto & point: contour) {
		polyline.addVertex(point.x, point.y);
	}
	polyline.close();
	return polyline;
}

ofRectangle boundsFromCvRect(const cv::Rect & rect) {
	return ofRectangle(rect.x, rect.y, rect.width, rect.height);
}

} // namespace

namespace ofxPotrace {

void RegionFinder::setImage(const ofPixels & pixels) {
	sourcePixels = pixels;
	sourcePixels.setImageType(OF_IMAGE_COLOR);
	regions.clear();
	maskPixels.clear();
}

void RegionFinder::clearImage() {
	sourcePixels.clear();
	maskPixels.clear();
	regions.clear();
}

bool RegionFinder::hasImage() const {
	return sourcePixels.isAllocated();
}

void RegionFinder::setSettings(const RegionFinderSettings & settings) {
	this->settings = settings;
}

RegionFinderSettings & RegionFinder::getSettings() {
	return settings;
}

const RegionFinderSettings & RegionFinder::getSettings() const {
	return settings;
}

bool RegionFinder::update() {
	regions.clear();
	maskPixels.clear();

	if(!sourcePixels.isAllocated() || sourcePixels.getWidth() == 0 || sourcePixels.getHeight() == 0) {
		return false;
	}

	cv::Mat rgb = pixelsToRgbMat(sourcePixels);
	cv::Mat converted;
	cv::Mat mask;

	const auto& band = settings.colorBand;

	if(band.model == ColorBandModel::Lab) {
		cv::cvtColor(rgb, converted, cv::COLOR_RGB2Lab);
		cv::Scalar lower(
			clampInt(band.lightnessMin, 0, 255),
			clampInt(band.aMin, 0, 255),
			clampInt(band.bMin, 0, 255));
		cv::Scalar upper(
			clampInt(band.lightnessMax, 0, 255),
			clampInt(band.aMax, 0, 255),
			clampInt(band.bMax, 0, 255));
		cv::inRange(converted, lower, upper, mask);
	} else {
		cv::cvtColor(rgb, converted, cv::COLOR_RGB2HSV);
		const int hueMin = clampInt(band.hueMin, 0, 179);
		const int hueMax = clampInt(band.hueMax, 0, 179);
		const int satMin = clampInt(band.saturationMin, 0, 255);
		const int satMax = clampInt(band.saturationMax, 0, 255);
		const int valMin = clampInt(band.valueMin, 0, 255);
		const int valMax = clampInt(band.valueMax, 0, 255);

		if(hueMin <= hueMax || !band.wrapHue) {
			cv::inRange(converted, cv::Scalar(hueMin, satMin, valMin), cv::Scalar(hueMax, satMax, valMax), mask);
		} else {
			cv::Mat lowerHueMask;
			cv::Mat upperHueMask;
			cv::inRange(converted, cv::Scalar(0, satMin, valMin), cv::Scalar(hueMax, satMax, valMax), lowerHueMask);
			cv::inRange(converted, cv::Scalar(hueMin, satMin, valMin), cv::Scalar(179, satMax, valMax), upperHueMask);
			cv::bitwise_or(lowerHueMask, upperHueMask, mask);
		}
	}

	if(settings.blurAmount > 0) {
		int kernelSize = settings.blurAmount * 2 + 1;
		cv::GaussianBlur(mask, mask, cv::Size(kernelSize, kernelSize), 0);
		cv::threshold(mask, mask, 127, 255, cv::THRESH_BINARY);
	}

	if(settings.openIterations > 0 || settings.closeIterations > 0) {
		cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
		if(settings.openIterations > 0) {
			cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), settings.openIterations);
		}
		if(settings.closeIterations > 0) {
			cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), settings.closeIterations);
		}
	}

	if(settings.invert) {
		cv::bitwise_not(mask, mask);
	}

	matToMaskPixels(mask, maskPixels);

	lastStats = {};
	lastStats.maskForegroundPixels = cv::countNonZero(mask);

	cv::Mat contourSrc = mask.clone();
	std::vector<std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(
		contourSrc,
		contours,
		hierarchy,
		settings.findHoles ? cv::RETR_CCOMP : cv::RETR_EXTERNAL,
		cv::CHAIN_APPROX_SIMPLE);

	lastStats.rawContours = static_cast<int>(contours.size());

	const float maxArea = settings.maxArea;
	for(std::size_t i = 0; i < contours.size(); ++i) {
		const float enclosedArea = static_cast<float>(std::abs(cv::contourArea(contours[i])));
		const cv::Rect box = cv::boundingRect(contours[i]);
		const float boxArea = static_cast<float>(box.width) * static_cast<float>(box.height);
		// Thin strokes have ~zero enclosed area; use bbox so line-like masks still count.
		const float metricArea = std::max(enclosedArea, boxArea);

		if(metricArea < settings.minArea) {
			continue;
		}
		if(maxArea > 0.0f && metricArea > maxArea) {
			continue;
		}

		std::vector<cv::Point> simplified;
		if(settings.simplifyTolerance > 0.0f) {
			cv::approxPolyDP(contours[i], simplified, settings.simplifyTolerance, true);
		} else {
			simplified = contours[i];
		}

		if(simplified.size() < 3) {
			continue;
		}

		Region region;
		region.contour = contourToPolyline(simplified);
		region.bounds = boundsFromCvRect(cv::boundingRect(simplified));
		region.area = metricArea;
		region.hole = settings.findHoles && !hierarchy.empty() && hierarchy[i][3] >= 0;
		regions.push_back(region);
	}

	lastStats.acceptedRegions = static_cast<int>(regions.size());

	std::sort(regions.begin(), regions.end(), [](const Region & a, const Region & b) {
		return a.area > b.area;
	});

	if(settings.maxContours > 0 && regions.size() > static_cast<std::size_t>(settings.maxContours)) {
		regions.resize(settings.maxContours);
	}

	return true;
}

const std::vector<Region> & RegionFinder::getRegions() const {
	return regions;
}

std::vector<ofPolyline> RegionFinder::getContours() const {
	std::vector<ofPolyline> contours;
	contours.reserve(regions.size());
	for(const auto & region: regions) {
		contours.push_back(region.contour);
	}
	return contours;
}

const ofPixels & RegionFinder::getMaskPixels() const {
	return maskPixels;
}

int RegionFinder::getWidth() const {
	return sourcePixels.getWidth();
}

int RegionFinder::getHeight() const {
	return sourcePixels.getHeight();
}

const RegionFinderStats & RegionFinder::getLastStats() const {
	return lastStats;
}

} // namespace ofxPotrace
