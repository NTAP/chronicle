// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FLOW_DESCRIPTOR_H
#define FLOW_DESCRIPTOR_H

#include <inttypes.h>
#include "ChronicleProcessRequest.h"

/* maximum TCP receive window size (assuming a max window scale option of 14)
   http://www.ietf.org/rfc/rfc1323.txt										*/
#define MAX_TCP_RECV_WINDOW							(1 << 30) //  XXX
#define FLOW_DESC_TCP_HEAD_WINDOW					200000

class Process;
class FlowDescriptorIPv4;

/**
 * A generic descriptor for IPv4 packets
 */
class FlowDescriptorIPv4 {
	public:
		FlowDescriptorIPv4(uint32_t sourceIP, uint32_t destIP, 
			uint16_t sourcePort, uint16_t destPort, uint8_t protocol);
		virtual ~FlowDescriptorIPv4();
		/**
		 * inserts a packet into the corresponding flow
		 * @param[in] pktDesc The packet buffer descriptor for the inserted packet
		 * @param[out] forceFlush whether all the packets in the descriptor should be flushed
		 * @returns true if packet is inserted successfully
		 */
		virtual bool insertPacketDescriptor(PacketDescriptor *pktDesc, 
			bool &forceFlush) = 0;
		/**
		 * tries to see if a packet matches to a flow
		 */
		bool match(uint32_t srcIP, uint32_t dstIP, uint16_t srcPort,
			uint16_t dstPort, uint8_t prot);
		bool match(PacketDescriptor *pktDesc);
		/**
		 * removes num packets from this flow
		 * @returns the chain of removed packets
		 */
		PacketDescriptor *releasePktDescriptors(uint8_t num);
		/**
		 * removes all packets from head to pktDesc
		 * @returns the chain of removed packets
		 */
		PacketDescriptor *releasePktDescriptors
			(PacketDescriptor *pktDesc, bool releaseLastPktDesc);
		/**
		 * removes all packets from firstPktDesc to lastPktDesc
		 * @returns the chain of removed packets
		 */
		PacketDescriptor *releasePktDescriptors
			(PacketDescriptor *firstPktDesc, PacketDescriptor *lastPktDesc,
			 bool releaseLastPktDesc);
		/**
		 * finds the next out of order packet
		 * @returns a pointer to the next out of order packet (possibly NULL)
		 */
		PacketDescriptor *findNextOutOfOrderPkt(PacketDescriptor *pktDesc);
		/**
		 * finds the first unscanned packet
		 * @returns a pointer to the next unscanned packet (possibly NULL)
		 */
		PacketDescriptor *findNextUnscannedPkt(PacketDescriptor *pktDesc);
		//bool duplicateEntryExists(PacketDescriptor *pktDesc);
		/**
		 * prints the list of packets maintained by the flow descriptor
		 */
		void print();
		/**
		 * prints the list of packets maintained by the flow descriptor in reverse
		 */
		void printReverse();
		/**
		 * prints the list of packets maintained by the flow descriptor with 
		 * TCP seq and ack numbers
		 */
		void printTcp();

		/// The number of packets in the queue of the descriptor
		uint16_t queueLen;
		/// The descriptor for the head of the packet list
		PacketDescriptor *head;
		/// The descriptor for the tail of the packet list
		PacketDescriptor *tail;
		/// The next flow (used for flows that hash to the same bucket in the flow table)
		FlowDescriptorIPv4 *next;
		/// The pointer to the flow descriptor holding packets in the reverse direction
		FlowDescriptorIPv4 *flowDescriptorReverse;
		/**
		 * first out of order packet (used by TCP but had to be modified in 
		 * releasePktDescriptors)
		 */
		PacketDescriptor *firstOutOfOrderPkt;
		/**
		 * first unscanned packet (used by RpcParser but had to be modified in 
		 * releasePktDescriptors)
		 */
		PacketDescriptor *firstUnscannedPkt;
		uint32_t sourceIP;
		uint32_t destIP;
		uint16_t sourcePort;
		uint16_t destPort;
		uint8_t protocol;
};

/**
 * A flow descriptor for TCP packets
 * FlowDescritorTcp uses a doubly linked list for storing packets. This choice
 * was made because this data structure has good performance for the common
 * case where packets are in order. When packets are in order, insertion and
 * deletion are O(1). In the worst case, runtime is O(n), where 
 * n=max(RPC_PARSER_GC_THRESH+RPC_PARSER_PACKETS_BATCH_SIZE,RPC_PARSER_REPLY_PARSE_THRESH).
 * A "compact list" can be used to alleviate the problem for the average case of
 * reordered packets. A balanced tree data structure with the average log(n)
 * performance for insertion, deletion, and update might be less efficient 
 * when out of order packets are relatively few. 
 */
class FlowDescriptorTcp : public FlowDescriptorIPv4 {
	public:
		FlowDescriptorTcp(uint32_t sourceIP, uint32_t destIP, 
			uint16_t sourcePort, uint16_t destPort, uint8_t protocol);
		virtual ~FlowDescriptorTcp();
		/**
		 * inserts a packet based on its sequence number in the list of packets
		 */
		bool insertPacketDescriptor(PacketDescriptor *pktDesc, 
			bool &forceFlush);
		/**
		 * handles a retransmitted packet
		 * @returns true if the retransmission fills a hole
		 */
		bool handleRetransmission(PacketDescriptor *pktDesc, 
			uint32_t maxSeqNumInPacket);
		/**
		 * inserts a packet before an out-of-order packet if it belongs there
		 * @returns true if the insert was successful
		 */
		bool insertPktInHole(PacketDescriptor *pktDesc, 
			PacketDescriptor *outOfOrderPktDesc);
		/**
		 * prints the stats for a flow descriptor
		 */
		void printStats();
		/**
		 * find duplicate packets in the packet stream
		 */
		bool findDuplicate(PacketDescriptor *pktDesc);
		/**
		 * validates the correctness of the list of packets maintained by the flow descriptor
		 * @params[inout] reverse The direction where flow table is invalid
		 * @returns true if the list is in the valid state
		 */
		bool isValid(bool &reverse);

		/// max sequence number seen for this flow
		uint32_t maxSeqNumSeen;
		/// max ACK number seen for this flow
		uint32_t maxAckNumSeen;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		uint64_t seenPacketCount;
		uint64_t outOfOrderCount;
		uint64_t emptyAckCount;
		uint64_t goodRetransCount;
		uint64_t badRetransCount;
		uint64_t wrapGoodRetransCount;
		uint64_t wrapBadRetransCount;
		uint64_t staleRetransCount;
		uint64_t recoveredOutOfOrderCount;
		uint64_t tailDiscardCount;
		uint64_t wrapDiscardCount;
		uint64_t miscDiscardCount;
		uint64_t truncatedPktCount;
		uint64_t droppedPktCount;
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
		uint64_t badRetransDuplicateCount;
		uint64_t badRetransNonDuplicateCount;
		#endif
};

#endif // FLOW_DESCRIPTOR_H

