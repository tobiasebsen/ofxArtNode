#pragma once

#include "ofMain.h"
#include "ArtNode.h"
#include "ArtDmx.h"

class ofxArtDmx : public ArtDmx {
public:
	ofxArtDmx() {
	}

	typedef struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} RGB;

	typedef struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t w;
	} RGBW;

	RGB * getDataRgb() {
		return (RGB*)Data;
	}

	RGBW * getDataRgbw() {
		return (RGBW*)Data;
	}

	void set(uint8_t v) {
		memset(Data, v, sizeof(Data));
	}

	void clear() {
		set(0);
	}
};