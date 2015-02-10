// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAP_PACKET_BUFFER_POOL_H
#define PCAP_PACKET_BUFFER_POOL_H

#include "PacketBufferPool.h"

/**
 * PcapPacketBufferPool is a singleton shared by all pcap readers
 */
class PcapPacketBufferPool : public PacketBufferPool {
	// no default copy constructor
	PcapPacketBufferPool(PcapPacketBufferPool &);
	// no default assignment operator
	PcapPacketBufferPool& operator=(const PcapPacketBufferPool &);

	public:
		~PcapPacketBufferPool();
		/**
		 * returns a buf pool instance (which has already been created through
		 * registerBufferPool hence the call order should properly be 
		 * maintained). It is used only by the PacketReader callback.
		 * @returns a pointer to the singleton pool buffer instance
		 */
		static PcapPacketBufferPool *getBufferPoolInstance();
		/**
		 * registers a process with the buf pool and, if it's the first user of 
		 * the pool, it allocates the pool
		 * @param[in] standardPktPoolSize buffer pool size for standard packets
		 * @param[in] jumboPktPoolSize buffer pool size for jumbo packets
		 * @returns a pointer to the singleton pool buffer instance
		 */
		static PcapPacketBufferPool *registerBufferPool(
			unsigned standardPktPoolSize = STAND_PKTS_IN_BUFFER_POOL,
			unsigned jumboPktPoolSize = JUMBO_PKTS_IN_BUFFER_POOL);
		/**
		 * unregisters a process from the buf pool
		 * @returns true if the process is the last user of buf pool and needs
		 * to deallocate the pool
		 */
		bool unregisterBufferPool();

	protected:
		PcapPacketBufferPool(unsigned standardPktPoolSize, 
			unsigned jumboPktPoolSize);
		/// atomically increases the number of buf pool users by 1
		void incrementRegisteredUsers();

	private:
		/// pointer to the singleton object
		static PcapPacketBufferPool *_bufPoolInstance;
		/// mutex lock used during buffer allocation
		static PthreadMutex _bufPoolMutex;
		/// pointer to a region of contiguously allocated packet buffers (standard size)
		PcapPacketBuffer *_pktBuffersStandard;
		/// pointer to a region of contiguously allocated packet buffers (jumbo size)
		PcapPacketBufferJumbo *_pktBuffersJumbo;
		/// the number of packet buffers in the pool
		unsigned _numPkts;
		/// the number of buf pool users
		unsigned _numRegisteredUsers;
};

#endif //PCAP_PACKET_BUFFER_POOL_H

