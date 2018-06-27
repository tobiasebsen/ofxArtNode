#include "ofxArtNode.h"

#ifdef WIN32
#include <iphlpapi.h>
#include <codecvt>
#else
#include <ifaddrs.h>
#endif

void ofxArtNode::setup(string host, string mask) {
    
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

	vector<string> msk = ofSplitString(mask, ".");
	if (msk.size() == 4) {
		for (int i=0; i<4; i++) {
			config->mask[i] = ofToInt(msk[i]);
		}
	}
	else {
		config->mask[0] = 255;
		config->mask[1] = 0;
		config->mask[2] = 0;
		config->mask[3] = 0;
	}

	config->udpPort = DefaultPort;

	udp.Create();
	udp.SetEnableBroadcast(true);
	udp.SetReuseAddress(true);
	udp.SetNonBlocking(true);
	udp.SetSendBufferSize(4096);
	udp.SetTimeoutSend(1);
	udp.ConnectMcast((char*)getBroadcastIp().c_str(), config->udpPort);
    
    pollInterval = 10000;

	ofLog() << "ArtNode setup on: " << getBroadcastIp();
}

map<string,string> ofxArtNode::getInterfaces() {
    map<string,string> interfaces;
#ifdef WIN32
	ULONG ulOutBufLen = 15*1024;
	PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), 0, ulOutBufLen);

	DWORD dwRetVal = GetAdaptersAddresses(AF_INET, 0, NULL, pAddresses, &ulOutBufLen);
	if (dwRetVal == NO_ERROR) {
		PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
			wstring wname = pCurrAddresses->FriendlyName;
			string name = converter.to_bytes(wname);
			string address;

			IP_ADAPTER_UNICAST_ADDRESS *pUniAddr = pCurrAddresses->FirstUnicastAddress;
			while (pUniAddr) {
				CHAR addrStr[32];
				DWORD addrStrLen = sizeof(addrStr);
				INT iRet = WSAAddressToStringA(pUniAddr->Address.lpSockaddr, pUniAddr->Address.iSockaddrLength, NULL, addrStr, &addrStrLen);
				if (iRet != SOCKET_ERROR) {
					address = addrStr;
					break;
				}
				pUniAddr = pUniAddr->Next;
			}
			interfaces[name] = address;
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	HeapFree(GetProcessHeap(), 0, pAddresses);
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
    unsigned long long now = ofGetSystemTime();
	int nbytes = udp.Receive((char*)this->getBufferData(), this->getBufferSize());
	if (nbytes > sizeof(ArtHeader) && isPacketValid()) {
		uint16_t opCode = getOpCode();
		if (opCode == OpPollReply) {
			ArtPollReply * reply = (ArtPollReply*)getBufferData();
			string addr;
			int port;
			udp.GetRemoteAddr(addr, port);
            
            NodeEntry ne;
            ne.address = addr;
            ne.timeStamp = now;
            ne.pollReply = *reply;
			nodes[addr] = ne;
            ofNotifyEvent(pollReplyReceived, ne, this);

            if (nodes.find(addr) == nodes.end())
                ofNotifyEvent(nodeAdded, ne, this);
        }
	}
    for (auto it=nodes.begin(); it!=nodes.end();) {
        NodeEntry & node = it->second;
        int age = node.timeStamp - lastPollTime;
        if (age > 4000) {
            ofNotifyEvent(nodeErased, node, this);
            it = nodes.erase(it);
            //it++;
        } else {
            it++;
        }
    }
    if (now - lastPollTime > pollInterval) {
        sendPoll();
    }
}

int ofxArtNode::getNumNodes() {
	return nodes.size();
}

int ofxArtNode::getNumRelpies() {
    int nreply = 0;
    for (auto & ne : nodes) {
        if (ne.second.timeStamp > lastPollTime) {
            nreply++;
        }
    }
    return nreply;
}

ArtPollReply * ofxArtNode::getNode(int index) {
	if (index > 0 && index < nodes.size()) {
		auto it = nodes.begin();
		advance(it, index);
		return &it->second.pollReply;
	}
	return NULL;
}

ArtPollReply * ofxArtNode::getNode(string addr) {
    auto it = nodes.find(addr);
    return it != nodes.end() ? &it->second.pollReply : NULL;
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
    lastPollTime = ofGetSystemTime();
    pollFrameNum = ofGetFrameNum();
}

void ofxArtNode::sendDmx(ArtDmx * dmx) {
	setPacketHeader((unsigned char*)dmx);
    bool nodeFound = sendUniCast(dmx->getNet(), dmx->getSub(), dmx->getUni(), (char*)dmx, dmx->getSize());
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
		ArtPollReply & reply = pair.second.pollReply;
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
    if (ofGetFrameNum() == pollFrameNum)
        return false;
    
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
