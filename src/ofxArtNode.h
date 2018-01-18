#pragma once

#include "ofMain.h"
#include "ArtNode.h"
#include "ofxArtDmx.h"
#include "ofxUDPManager.h"

class ofxArtNode : public ArtNode {
public:
    static map<string,string> getInterfaces();
    static string getInterfaceAddr(string name);
	static string getInterfaceAddr(int ifindex);

    void setup(string host = "2.255.255.255");

	void update();

	int getNumNodes();
	ArtPollReply * getNode(int index);
    ArtPollReply * getNode(string addr);
	string getNodeIp(int index);

	void sendPoll();
	void sendDmx(ArtDmx * dmx);
	void sendSync();

	ofxArtDmx * createArtDmx(int net, int sub, int universe);

	void sendMultiCast(char * data, int length);
	void sendMultiCast();
	bool sendUniCast(int net, int subnet, int universe, char * data, int length);
	bool sendUniCast(int net, int subnet, int universe);
    bool sendUniCast(string addr, unsigned short udpPort, char * data, int length);
    bool sendUniCast(string addr, unsigned short udpPort = 0x1936);

	bool readyFps(float frameRate);
	void doneFps();
    
    ofEvent<string> pollReplyReceived;

protected:
	ofxUDPManager udp;
	map<string,ArtPollReply> nodes;
	uint64_t lastFrameTime;

	string getBroadcastIp();
};