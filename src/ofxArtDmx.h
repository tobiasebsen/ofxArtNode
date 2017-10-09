#pragma once

#include "ofMain.h"
#include "ArtDmx.h"

class ofxArtDmx : public ArtDmx {
public:

	struct RGB {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	struct RGBW {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t w;
	};

	RGB * getDataRgb() {
		return (RGB*)Data;
	}

	RGBW * getDataRgbw() {
		return (RGBW*)Data;
	}
};