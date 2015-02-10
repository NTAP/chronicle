// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <inttypes.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sstream>
#include "NetworkHeaderParser.h"
#include "Message.h"
#include "PcapPacketBufferPool.h"
#include "Chronicle.h"
#include "ChroniclePipeline.h"
#include "StatGatherer.h"
#include "PacketReader.h"
#include "FDWatcher.h"
#include "Interface.h"

void printIpAddress(uint32_t addr);
uint32_t bitRange(uint32_t num, uint8_t start, uint8_t end);

class NetworkHeaderParser::MsgProcessQueue : public Message {
	private:
		NetworkHeaderParser *_parser;

	public:
		MsgProcessQueue(NetworkHeaderParser *parser) : _parser(parser) { }
		void run() { _parser->doHandleFdWatcher(); }
};

class NetworkHeaderParser::FDWatcherCb : public FDWatcher::FdCallback {
	private:
		NetworkHeaderParser *_parser;

	public:
		FDWatcherCb(NetworkHeaderParser *parser) : _parser(parser) { }
		void operator()(int fd, FDWatcher::Events event) {
			_parser->handleFdWatcher();
		}
};

class NetworkHeaderParser::NetHdrParserMsg {
	public:
		typedef enum actions {
			NP_READ,
			NP_SHUTDOWN
		} NetHdrParserAction;
		NetHdrParserMsg(NetHdrParserAction action, 
			PacketDescriptor *pktDesc = NULL)
			: _action(action), _pktDesc(pktDesc) { }
	
		NetHdrParserAction _action;
		PacketDescriptor *_pktDesc;
};

class NetworkHeaderParser::NetHdrParserQueueCb : 
		public FDQueue<NetHdrParserMsg>::FDQueueCallback {
	private:
		NetworkHeaderParser *_parser;

	public:
		NetHdrParserQueueCb(NetworkHeaderParser *parser) 
			: _parser(parser) { }
		~NetHdrParserQueueCb() { }
		void operator ()(NetHdrParserMsg *msg) {
			switch(msg->_action) {
				case NetHdrParserMsg::NP_READ:
					_parser->doProcessRequest(msg->_pktDesc);
					break;
				case NetHdrParserMsg::NP_SHUTDOWN:
					_parser->doShutdownProcess();
					break;
				default:
					;
			}
			delete msg;
		}
};

NetworkHeaderParser::NetworkHeaderParser(PacketReader *reader, 
		bool pcapPipeline, Chronicle *chronicle) :
	Process("NetworkHeaderParser"), _packetReader(reader), 
	_pcapPipeline(pcapPipeline), _chronicle(chronicle), _lastStatCall(0)
{
	_queueCb = new NetHdrParserQueueCb(this);
	_selfFdQueue = new FDQueue<NetHdrParserMsg> (_queueCb);
	_queueFd = _selfFdQueue->getQueueFd();
	_fdWatcherCb = new FDWatcherCb(this);
	FDWatcher::getFDWatcher()->registerFdCallback(
		_queueFd, FDWatcher::EVENT_READ, _fdWatcherCb); 
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		_packetReader->processDone(this);
}

NetworkHeaderParser::~NetworkHeaderParser()
{
	delete _queueCb;	
	delete _selfFdQueue;
	delete _fdWatcherCb;
}

void
NetworkHeaderParser::handleFdWatcher()
{
	enqueueMessage(new MsgProcessQueue(this));
}

void
NetworkHeaderParser::doHandleFdWatcher()
{
	FDWatcher::getFDWatcher()->registerFdCallback(
		_queueFd, FDWatcher::EVENT_READ, _fdWatcherCb); 
	_selfFdQueue->dequeue();
}

void
NetworkHeaderParser::processRequest(PacketReader *reader, 
	PacketDescriptor *pktDesc)
{
	_selfFdQueue->enqueue(new NetHdrParserMsg(NetHdrParserMsg::NP_READ, 
		pktDesc));
}

void
NetworkHeaderParser::doProcessRequest(PacketDescriptor *pktDesc)
{
	PacketDescriptor *tmpPktDesc;

    if (_chronicle && pktDesc) {
        timeval *tv = &(pktDesc->pcapHeader.ts);
        uint64_t ts = static_cast<uint64_t>(tv->tv_sec) * 1000000000llu +
        tv->tv_usec * 1000;
        if (ts > _lastStatCall + 1000000000llu) { // 1s
            _chronicle->getStatGatherer()->tick();
            _lastStatCall = ts;
        }
    }

	while (pktDesc != NULL) {
		if (parseNetworkHeader(pktDesc) || _pcapPipeline) {
			// send all parsable packets to the appropriate pipeline
			ChroniclePipeline *pipeline = 
				PipelineManager::findPipeline(pktDesc->srcIP, pktDesc->destIP,
					pktDesc->srcPort, pktDesc->destPort, pktDesc->protocol);
			PacketDescReceiver *nextProcess = 
				pipeline->getPipelineHeadProcess();
			tmpPktDesc = pktDesc;
			pktDesc = pktDesc->next;
			// necessary if the next process is PcapWriter
			tmpPktDesc->prev = tmpPktDesc->next = NULL; 
			nextProcess->processRequest(NULL, tmpPktDesc);
			continue;
		}
		tmpPktDesc = pktDesc;
		pktDesc = pktDesc->next;
		assert(_bufPool->releasePacketDescriptor(tmpPktDesc));
	}
}

void
NetworkHeaderParser::shutdown(PacketReader *src)
{
	if (src == _packetReader)
		_selfFdQueue->enqueue(new NetHdrParserMsg(NetHdrParserMsg::NP_SHUTDOWN));
}

void
NetworkHeaderParser::doShutdownProcess()
{
	FDWatcher::getFDWatcher()->clearFdCallback(_queueFd);
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;
	_packetReader->shutdownDone(this);
	exit();
}

bool
NetworkHeaderParser::parseNetworkHeader(PacketDescriptor *pktDesc)
{
	unsigned char *ethFrame = pktDesc->packetBuffer;
	int index = 12;
	uint16_t L3Protocol = UINT16_BIG2HOST(ethFrame, index);
	if (L3Protocol == 0x8100) { // an IEEE 802.1Q-tagged frame
		index += 4;
		L3Protocol = UINT16_BIG2HOST(ethFrame, index);
	} else if (L3Protocol == 0x88a8) { // an IEEE 802.1QinQ tagged frame
		index += 8;
		L3Protocol = UINT16_BIG2HOST(ethFrame, index);
	}
	if (L3Protocol <= ETHERNET_MTU) { // IEEE 802.3 frame
		/* RFC 1042 */ //TODO: validate
		if ((ethFrame[index] == 0xaa || ethFrame[index] == 0xab)
			&& (ethFrame[index+1] == 0xaa || ethFrame[index+1] == 0xab)
			&& ethFrame[index+3] == 0 
			&& ethFrame[index+4] == 0
			&& ethFrame[index+5] == 0) {
			index += 6;
			L3Protocol = UINT16_BIG2HOST(ethFrame, index);
		}
	} else if (L3Protocol > 0x0600) { // Ethernet 2 frame
		;
	} else 
		return false;
	if (L3Protocol == 0x0800) { // IP packet
		index += 2;
		return parseIpDatagram(pktDesc, index);
	} else if (L3Protocol == 0x8870 || L3Protocol == 0x8847 
		|| L3Protocol == 0x8848) { // jumbo frame
		// TODO: handling jumbo frames
		return false;
	} else
		return false;
}

bool
NetworkHeaderParser::parseIpDatagram(PacketDescriptor *pktDesc, 
	int &index)
{
	unsigned char *ethFrame = pktDesc->packetBuffer;
	uint8_t ipHeaderOffset = index;
	if (ethFrame[index] & 0x40 ) { // IPv4 packet
		uint8_t ipHeaderLen = (ethFrame[index] & 0x0f) << 2;
		index += 2;
		uint16_t ipDatagramSize = UINT16_BIG2HOST(ethFrame, index);
		index += 4;
		if ((ethFrame[index] == 0 && ethFrame[index+1] == 0)
				|| (ethFrame[index] == 0x40 && ethFrame[index+1] == 0)) {
			index += 3;
			pktDesc->protocol = ethFrame[index];
			if (pktDesc->protocol == IPPROTO_TCP) { // TCP packet
				index += 3;
				pktDesc->srcIP = UINT32_BIG2HOST(ethFrame, index);
				index += 4;
				pktDesc->destIP = UINT32_BIG2HOST(ethFrame, index); 
				index += 4;
				index += ipHeaderLen - (index - ipHeaderOffset);
				uint16_t tcpSegmentSize = ipDatagramSize - 
					(index - ipHeaderOffset);
				return parseTcpSegment(pktDesc, index, tcpSegmentSize);
			} else if (pktDesc->protocol == IPPROTO_UDP) { // UDP packet
				//TODO: handling UDP packets
				return false;
			} else { // other L4 packets
				return false;
			}
		} else { // fragmented IP packet
			// TODO: handling fragmented IP packets
			return false;
		}
	} else if (ethFrame[index] & 0x60 ) {// IPv6 packet
		// TODO: handling IPv6
		return false;
	} else 
		return false;
}

bool
NetworkHeaderParser::parseTcpSegment(PacketDescriptor *pktDesc, int &index,
	uint16_t tcpSegmentSize)
{
	unsigned char *ethFrame = pktDesc->packetBuffer;
	uint8_t tcpHeaderOffset = index;
	if (tcpSegmentSize > pktDesc->pcapHeader.caplen) {
		printf("NetworkHeaderParser::parseTcpSegment: Dropping bad packet - "
			"tcpSegmentSize:%u pktDesc->pcapHeader.len:%u pktDesc->pcapHeader.caplen:%u\n", 
			tcpSegmentSize, pktDesc->pcapHeader.len, pktDesc->pcapHeader.caplen);
		//assert(false);
		return false; //TODO	
	}	
	// handling truncated packets (mostly due to pkt > MTU)
	if ((unsigned)index + tcpSegmentSize > pktDesc->pcapHeader.caplen)
		tcpSegmentSize -= pktDesc->pcapHeader.len - pktDesc->pcapHeader.caplen;
	
	pktDesc->srcPort = UINT16_BIG2HOST(ethFrame, index);
	index += 2;
	pktDesc->destPort = UINT16_BIG2HOST(ethFrame, index);
	index += 2;
	pktDesc->tcpSeqNum = UINT32_BIG2HOST(ethFrame, index);
	index += 4;
	pktDesc->tcpAckNum = UINT32_BIG2HOST(ethFrame, index);
	index += 4;
	uint8_t dataOffset = (ethFrame[index] >> 4) << 2;
	index += 8;
	index += dataOffset - (index - tcpHeaderOffset);
	pktDesc->toParseOffset = pktDesc->payloadOffset = index;
	pktDesc->payloadLength = tcpSegmentSize - 
		(pktDesc->payloadOffset - tcpHeaderOffset);
	if (pktDesc->payloadLength > MAX_ETH_FRAME_SIZE_JUMBO) {
		printf("pktDesc->pcapHeader.len:%u pktDesc->pcapHeader.caplen:%u "
			"tcpSegmentSize:%u pktDesc->payloadLength:%u\n",
			pktDesc->pcapHeader.len, pktDesc->pcapHeader.caplen,
			tcpSegmentSize, pktDesc->payloadLength);
		assert(false);
	}
	return true;
}

std::string 
NetworkHeaderParser::getId()
{
	return std::string(_packetReader->getInterface()->getName()); 
}

void 
printIpAddress(uint32_t addr)
{
	std::cout << bitRange(addr, 24, 31) << "."
		<< bitRange(addr, 16, 23) << "."
		<< bitRange(addr, 8, 15) << "."
		<< bitRange(addr, 0, 7);
}

uint32_t 
bitRange(uint32_t num, uint8_t start, uint8_t end)
{
	uint8_t shiftLeft, shiftRight;
	if (start > 31 || end < start || end > 31)
		fprintf(stderr, "getBitRange: invalid range\n");
	shiftLeft = sizeof(uint32_t) * 8 - 1 - end;
	shiftRight = start + shiftLeft;
	return (num << shiftLeft) >> shiftRight; 
}

std::string 
getIpDotNotation(uint32_t addr)
{
	std::ostringstream ip;
	ip << bitRange(addr, 24, 31) << "."
		<< bitRange(addr, 16, 23) << "."
		<< bitRange(addr, 8, 15) << "."
		<< bitRange(addr, 0, 7);
	return ip.str();
}

