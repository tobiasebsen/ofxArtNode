#include "ofxArtNode.h"

void ofxArtNode::setup(string host) {

	config->ip[0] = NETID0;
	config->ip[1] = 255;
	config->ip[2] = 255;
	config->ip[3] = 255;

	config->mask[0] = 255;
	config->mask[1] = 0;
	config->mask[2] = 0;
	config->mask[3] = 0;

	config->udpPort = DefaultPort;

	udp.Create();
	udp.SetEnableBroadcast(true);
	udp.SetReuseAddress(true);
	udp.SetNonBlocking(true);
	udp.SetSendBufferSize(4096);
	udp.SetTimeoutSend(1);
	udp.ConnectMcast((char*)getBroadcastIp().c_str(), config->udpPort);

	ofLog() << "ArtNode setup on: " << getBroadcastIp();
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

int ofxArtNode::getNumNodes() {
	return nodes.size();
}

ArtPollReply * ofxArtNode::getNode(int index) {
	if (index > 0 && index < nodes.size()) {
		auto it = nodes.begin();
		advance(it, index);
		return &it->second;
	}
	return NULL;
}

string ofxArtNode::getNodeIp(int index) {
	if (index > 0 && index < nodes.size()) {
		auto it = nodes.begin();
		advance(it, index);
		return it->first;
	}
	return string();
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
	udp.Connect(getBroadcastIp().c_str(), config->udpPort);
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
					udp.Connect(addr.c_str(), reply.BoxAddr.Port != 0 ? reply.BoxAddr.Port : config->udpPort);
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

void ofxArtNode::doneFps() {
	lastFrameTime = ofGetElapsedTimeMillis();
}

string ofxArtNode::getBroadcastIp() {
	in_addr bc;
	bc.S_un.S_addr = broadcastIP();
	return inet_ntoa(bc);
}
