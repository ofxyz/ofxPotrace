meta:
	ADDON_NAME = ofxPotrace
	ADDON_DESCRIPTION = libpotrace vector tracing and OpenCV region finding for openFrameworks.
	ADDON_AUTHOR = openFrameworks community
	ADDON_TAGS = "computer vision" "contours" "tracing" "vector"
	ADDON_URL = https://github.com/openframeworks/openFrameworks

common:
	ADDON_DEPENDENCIES = ofxEnTTKit ofxEnTTInspector ofxOpenCv ofxLibTess2
	ADDON_INCLUDES += libs/potrace
	ADDON_CPPFLAGS += -DVERSION=\"1.16\"
	ADDON_LDFLAGS += -lm
	ADDON_SOURCES += libs/potrace/potracelib.c libs/potrace/curve.c libs/potrace/trace.c libs/potrace/decompose.c
	ADDON_SOURCES_EXCLUDE += libs/potrace-upstream/%

linux64:
linux:
linuxarmv6l:
linuxarmv7l:
linuxaarch64:
msys2:
vs:
osx:
ios:
tvos:
android/armeabi:
android/armeabi-v7a:
android/arm64-v8a:
emscripten:
