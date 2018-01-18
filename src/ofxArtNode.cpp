#include "ofxArtNode.h"

#ifdef WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#endif

void ofxArtNode::setup(string host) {
    
    vector<string> addr = ofSplitString(host, ".");
    if (addr.size() == 4) {
        for (int i=0; i<4; i++) {
            config->ip[i] = ofToInt(addr[i]);
        }
    }
    else {
        config->ip[0] = NETID0;
        config->ip[1] = 255;
        config->ip[2] = 255;
        config->ip[3] = 255;
    }

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

map<string,string> ofxArtNode::getInterfaces() {
    map<string,string> interfaces;
#ifdef WIN32
	PIP_ADAPTER_INFO pAdapterInfo;
	ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);

	if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
	}

	DWORD dwRetVal = 0;
	if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
		PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
		while (pAdapter) {
			string name = pAdapter->AdapterName;
			string host = pAdapter->IpAddressList.IpAddress.String;
			interfaces[name] = host;
			pAdapter = pAdapter->Next;
		}
	}
	
	free(pAdapterInfo);
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return;
    }
    
    ifa = ifaddr;
    while (ifa->ifa_next != NULL) {
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char host[NI_MAXHOST];
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
            
            interfaces[ifa->ifa_name] = host;
        }

        ifa = ifa->ifa_next;
    }
    freeifaddrs(ifaddr);
#endif
    return interfaces;
}

string ofxArtNode::getInterfaceAddr(string name) {
    map<string,string> interfaces = getInterfaces();
    auto it = interfaces.find(name);
    return it == interfaces.end() ? "2.255.255.255" : it->second;
}

string ofxArtNode::getInterfaceAddr(int ifindex) {
	map<string,string> interfaces = getInterfaces();
	if (ifindex < interfaces.size()) {
		auto it = interfaces.begin();
		advance(it, ifindex);
		return it->second;
	}
	else
		return "";
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
            ofNotifyEvent(pollReplyReceived, addr, this);
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

ArtPollReply * ofxArtNode::getNode(string addr) {
    auto it = nodes.find(addr);
    return it != nodes.end() ? &it->second : NULL;
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
    bool nodeFound = sendUniCast(dmx->getNet(), dmx->getSub(), dmx->getUni(), (char*)dmx, sizeof(ArtDmx));
    //sendMultiCast((char*)dmx, sizeof(ArtDmx));
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
					sendUniCast(addr.c_str(), reply.BoxAddr.Port != 0 ? reply.BoxAddr.Port : config->udpPort, data, length);
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

bool ofxArtNode::sendUniCast(string addr, unsigned short udpPort, char * data, int length) {
    if (!udp.Connect(addr.c_str(), udpPort))
		return false;

    if (udp.SendAll(data, length) < length)
		return false;

	return true;
}

bool ofxArtNode::sendUniCast(string addr, unsigned short udpPort) {
    return sendUniCast(addr, udpPort, (char*)getBufferData(), getPacketSize());
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
#ifdef WIN32
	bc.S_un.S_addr = broadcastIP();
#else
    bc.s_addr = broadcastIP();
#endif
	return inet_ntoa(bc);
}
