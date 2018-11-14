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

    void setup(string host = "2.255.255.255", string mask = "255.0.0.0");

	void update();

	int getNumNodes();
    int getNumRelpies();
	ArtPollReply * getNode(int index);
    ArtPollReply * getNode(string addr);
	string getNodeIp(int index);

	void sendPoll();
	void sendDmx(ArtDmx * dmx);
	void sendSync();

	ofxArtDmx * createArtDmx(int net = 0, int sub = 0, int universe = 0);

	void sendMultiCast(char * data, int length);
	void sendMultiCast();
	bool sendUniCast(int net, int subnet, int universe, char * data, int length);
	bool sendUniCast(int net, int subnet, int universe);
    bool sendUniCast(string addr, unsigned short udpPort, char * data, int length);
    bool sendUniCast(string addr, unsigned short udpPort = 0x1936);

	bool readyFps(float frameRate);
	void doneFps();
    
    typedef struct _NodeEntry {
        string address;
        unsigned long long timeStamp;
        ArtPollReply pollReply;
    } NodeEntry;

    ofEvent<NodeEntry> pollReplyReceived;
    ofEvent<NodeEntry> nodeAdded;
    ofEvent<NodeEntry> nodeErased;

protected:
	ofxUDPManager udp;
    
	map<string,NodeEntry> nodes;
	uint64_t lastFrameTime;
    uint64_t lastPollTime;
    uint32_t pollInterval;
    uint64_t pollFrameNum;

	string getBroadcastIp();
};