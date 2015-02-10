// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef NETWORK_HEADER_PARSER_H
#define NETWORK_HEADER_PARSER_H

#include "Process.h"
#include "ChronicleProcess.h"
#include "FDQueue.h"

#define MIN(a,b)							(a < b) ? a : b
#define ETHERNET_MTU						1500

class PcapPacketBufferPool;
class Chronicle;
class PacketReader;

class NetworkHeaderParser : public Process {
	public:
		NetworkHeaderParser(PacketReader *reader, bool basicPipeline, 
			Chronicle *chronicle);
		~NetworkHeaderParser();
		void handleFdWatcher();
		void processRequest(PacketReader *reader, PacketDescriptor *pktDesc);
		void shutdown(PacketReader *reader);
		bool parseNetworkHeader(PacketDescriptor *pktDesc);
		std::string getId();

	private:
		class MsgProcessQueue;
		class FDWatcherCb;
		class NetHdrParserQueueCb;
		class NetHdrParserMsg;
		
		void doHandleFdWatcher();
		void doProcessRequest(PacketDescriptor *pktDesc);
		void doShutdownProcess();
		bool parseIpDatagram(PacketDescriptor *pktDesc, int &index);
		bool parseTcpSegment(PacketDescriptor *pktDesc, int &index, 
			uint16_t tcpSegmentSize);

		/// the link to the corresponding PacketReader
		PacketReader *_packetReader;
		/// reference to packet buffer pool
		PcapPacketBufferPool *_bufPool;
		/// basic pipeline (pcap with no parsing)
		bool _pcapPipeline;
		/// queue for receiving messages from PacketReader
		FDQueue<NetHdrParserMsg> *_selfFdQueue;
		/// file descriptor associated with the queue
		int _queueFd;
		/// callback for the communication queue
		NetHdrParserQueueCb *_queueCb;
		/// FDWatcher callback
		FDWatcherCb *_fdWatcherCb;
		/// handle to supervisor
		Chronicle *_chronicle;
		/// the time of last call to StatGatherer
		uint64_t _lastStatCall;
};

#endif // NETWORK_HEADER_PARSER_H
