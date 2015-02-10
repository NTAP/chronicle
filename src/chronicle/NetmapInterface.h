// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef NETMAP_INTERFACE_H
#define NETMAP_INTERFACE_H

#define NETMAP_WITH_LIBS

#include <list>
#include <poll.h>
#include <pcap.h>
#include <net/if.h>
#include <string>
#include "Interface.h"
#include "ChronicleConfig.h"
#include "PcapPacketBufferPool.h"
#include "netmap.h"
#include "netmap_user.h"

/**
 * Defines a Netmap interface
 */
class NetmapInterface : public Interface {
	public:
		NetmapInterface(const char *name, 
			uint32_t batchSize = PCAP_DEFAULT_BATCH_SIZE, bool toCopy = true,
			int snapLen = MAX_ETH_FRAME_SIZE_JUMBO, 
			int timeOut = PCAP_DEFAULT_TIMEOUT_LEN);
		~NetmapInterface();
		int open();
		InterfaceStatus read();
		void close();
		char *getFilter() { return NULL; }
		bool getToCopy() { return _toCopy; }
		void getNicStats() { } 
		void printError(std::string operation);
		uint32_t printPackets(struct netmap_ring *ring);
		void processPackets(uint16_t ringId, struct netmap_ring *ring);
	
	private:
		/// corresponding NIC (different from interface name)
		char _interface[IFNAMSIZ];
		/// netmap request structure
		struct nmreq _netmapReq;
		/// netmap interface
		struct netmap_if *_netmapIf;
		/// pointer to the pcap packet buffer pool singleton
		PcapPacketBufferPool *_bufPool;
		/// mapped netmap buffers' address
		void *_bufAddr;
		/// set of file descriptors being monitored
		struct pollfd _fds[2]; //TODO: change for multi-queue NICs
		/// buffer for error message
		char _errBuf[NM_ERRBUF_SIZE];
		/// file descriptor for the netmap device
		int _fd;
		/// read timeout value in millisecond
		int _timeOut;
		/// number of rx rings
		uint16_t _numRxRings;
		/// the ID for the first rx ring
		uint16_t _firstRxRing;
		/// the ID for the last rx ring
		uint16_t _lastRxRing;
		/// drop interval
		uint16_t _dropInterval;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_PKTLOSS)
		#define MAX_INTERVAL 21
		/// next drop
		uint16_t _nextDrop;
		/// current count
		uint16_t _curCount;
		/// current interval
		uint8_t _curInterval;
		/// loss rates out of 1000 packets
		uint16_t _intervalLossRate[MAX_INTERVAL];
		/// time interval start time
		time_t _intervalStartTime[MAX_INTERVAL];
		/// time interval duration
		uint32_t _intervalDuration[MAX_INTERVAL];
		#endif
};

#endif //NETMAP_INTERFACE_H

