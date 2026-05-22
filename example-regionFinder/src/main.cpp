#include "ofMain.h"
#include "ofApp.h"

int main() {
	ofGLFWWindowSettings settings;
	settings.setSize(1280, 800);
	settings.windowMode = OF_WINDOW;
	settings.title = "ofxPotrace - Region Finder";
	settings.resizable = true;

	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
