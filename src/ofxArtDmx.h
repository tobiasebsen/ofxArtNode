#pragma once

#include "ofMain.h"
#include "ArtNode.h"
#include "ArtDmx.h"

class ofxArtDmx : public ArtDmx {
public:
	ofxArtDmx() {
		ArtNode::setPacketHeader((unsigned char*)this);
		this->OpCode = OpDmx;
		this->ProtVerHi = 0;
		this->ProtVerLo = ProtocolVersion;
		this->Sequence = 0;
		this->Physical = 0;
		this->Net = 0;
		this->SubUni = 0;
        setLength(512);
	}

	typedef struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} RGB;

    typedef struct {
        uint8_t g;
        uint8_t r;
        uint8_t b;
    } GRB;

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
    
    void set(GRB * from, uint16_t count) {
        RGB * to = getDataRgb();
        for (int i=0; i<count; i++) {
            to[i].r = from[i].r;
            to[i].g = from[i].g;
            to[i].b = from[i].b;
        }
    }

	void set(uint8_t v) {
		memset(Data, v, sizeof(Data));
	}

	void clear() {
		set(0);
	}
};