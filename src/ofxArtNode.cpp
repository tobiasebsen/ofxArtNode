#include "ofxArtNode.h"

void ofxArtNode::setup() {
	udp.Create();
	udp.SetEnableBroadcast(true);
	udp.SetReuseAddress(true);
	udp.SetNonBlocking(true);
	udp.SetSendBufferSize(4096);
	udp.SetTimeoutSend(1);
	udp.ConnectMcast("2.255.255.255", 0x1936);
}

void ofxArtNode::update() {
	int nbytes = udp.Receive((char*)this->getBufferData(), this->getBufferSize());
	if (nbytes > sizeof(ArtHeader) && isPacketValid()) {
		uint16_t opCode = getOpCode();
		if (opCode == OpPollReply) {
			ArtPollReply * reply = (ArtPollReply*)getBufferData();
			string addr;
			int port;
			udp.GetRemoteAddr(addr, port);
			nodes[addr] = *reply;
		}
	}
}

void ofxArtNode::sendPoll() {
	createPoll();
	sendMultiCast();
}

void ofxArtNode::sendDmx(ArtDmx * dmx) {
	if (!sendUniCast(dmx->getNet(), dmx->getSub(), dmx->getUni(), (char*)dmx, sizeof(ArtDmx)))
		sendMultiCast((char*)dmx, sizeof(ArtDmx));
}

void ofxArtNode::sendSync() {
	createSync();
	sendMultiCast();
}

ofxArtDmx * ofxArtNode::createArtDmx(int net, int sub, int universe) {
	return (ofxArtDmx*)createDmx((uint8_t)net, (uint8_t)((sub << 4) | universe));
}

void ofxArtNode::sendMultiCast(char * data, int length) {
	udp.Connect(getBroadcastIp().c_str(), 0x1936);
	udp.SendAll(data, length);
}

void ofxArtNode::sendMultiCast() {
	sendMultiCast((char*)getBufferData(), getPacketSize());
}

bool ofxArtNode::sendUniCast(int net, int subnet, int universe, char * data, int length) {
	bool ret = false;
	for (auto & pair : nodes) {
		string addr = pair.first;
		ArtPollReply & reply = pair.second;
		if (reply.NetSwitch == net && reply.SubSwitch == subnet) {
			for (int i=0; i<reply.NumPortsLo; i++) {
				if (reply.PortTypes[i] & PortTypeOutput && reply.getPortProtocol(i) == PortTypeDmx && reply.SwOut[i] == universe) {
					udp.Connect(addr.c_str(), 0x1936);
					udp.SendAll(data, length);
					ret = true;
				}
			}
		}
	}
	return ret;
}

bool ofxArtNode::sendUniCast(int net, int subnet, int universe) {
	return sendUniCast(net, subnet, universe, (char*)getBufferData(), getPacketSize());
}

bool ofxArtNode::readyFps(float frameRate) {
	uint64_t now = ofGetElapsedTimeMillis();
	if (now - lastFrameTime >= 1000 / frameRate) {
		lastFrameTime = now;
		return true;
	}
	return false;
}

string ofxArtNode::getBroadcastIp() {
	char sz[16];
	uint32_t bc = broadcastIP();
	sprintf(sz, "%d.%d.%d.%d", bc >> 24, (bc >> 16) & 0xF, (bc >> 8) & 0xF, bc & 0xF);
	return sz;
}
