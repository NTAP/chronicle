// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PACKET_BUFFER_POOL_H
#define PACKET_BUFFER_POOL_H

#include <string>
#include "TSQueue.h"
#include "PacketBuffer.h"
#include "Lock.h"

#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
#include <atomic>
#endif

class PacketDescriptor;

/**
 * Used when a packet buffer pool cannot be allocated properly
 */
class PacketBufPoolException
{
	public:
		PacketBufPoolException(std::string msg) 
			: errMsg(msg) { }
		std::string getErrorMsg() { return errMsg; }
	
	private:
		std::string errMsg;
		PacketBufPoolException() {}; 
};

/**
 * The abstract base class for packet buffer pool
 */
class PacketBufferPool {
	public:
		virtual ~PacketBufferPool();
		/// returns a packet buffer descriptor from the free list
		PacketDescriptor *getPacketDescriptor(unsigned ethFrameSize);
		/// releases a packet buffer descriptor back to the free list
		bool releasePacketDescriptor(PacketDescriptor *pktDesc);
		/// prints a packet buffer descriptor given the index in the buf pool
		void print(unsigned index);
		/// unregisters a buffer pool user
		virtual bool unregisterBufferPool() = 0;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)	
		void getBufPoolStats(uint64_t &allocatedBufsStandard, 
			uint64_t &maxAllocdBufsStandard, uint64_t &timesNoFreeBufsStandard,
			uint64_t &allocatedBufsJumbo, uint64_t &maxAllocatedBufsJumbo,
			uint64_t &timesNoFreeBufsJumbo);
		#endif

	protected:
		PacketBufferPool(unsigned standardPktPoolSize, 
			unsigned jumboPktPoolSize);
		/// the FIFO list for standard size free packet buffer descriptors
		TSQueue<PacketDescriptor> *_pktDescFreeListStandard;
		/// the FIFO list for jumbo size free packet buffer descriptors
		TSQueue<PacketDescriptor> *_pktDescFreeListJumbo;
		/// pointer to the contiguously allocated region of packet buffer descriptors
		PacketDescriptor *_pktDescriptors;
		/// buffer pool size for standard packets
		unsigned _standardPktPoolSize;
		/// buffer pool size for jumbo packets
		unsigned _jumboPktPoolSize;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
		std::atomic<uint64_t> _allocatedBufsStandard;
		std::atomic<uint64_t> _maxAllocatedBufsStandard;
		std::atomic<uint64_t> _timesNoFreeBufsStandard;		
		std::atomic<uint64_t> _allocatedBufsJumbo;
		std::atomic<uint64_t> _maxAllocatedBufsJumbo;
		std::atomic<uint64_t> _timesNoFreeBufsJumbo;	
		#endif
};

#endif //PACKET_BUFFER_POOL_H

