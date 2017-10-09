#pragma once

#include "ofMain.h"
#include "ArtNode.h"
#include "ofxArtDmx.h"
#include "ofxUDPManager.h"

class ofxArtNode : public ArtNode {
public:
	void setup();

	void update();

	void sendPoll();
	void sendDmx(ArtDmx * dmx);
	void sendSync();

	ofxArtDmx * createArtDmx(int net, int sub, int universe);

	void sendMultiCast(char * data, int length);
	void sendMultiCast();
	bool sendUniCast(int net, int subnet, int universe, char * data, int length);
	bool sendUniCast(int net, int subnet, int universe);

	bool readyFps(float frameRate);

protected:
	ofxUDPManager udp;
	map<string,ArtPollReply> nodes;
	uint64_t lastFrameTime;

	string getBroadcastIp();
};