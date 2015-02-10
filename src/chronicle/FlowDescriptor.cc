// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <assert.h>
#include "FlowDescriptor.h"

extern void printIpAddress(uint32_t addr);

FlowDescriptorIPv4::FlowDescriptorIPv4(uint32_t sourceIP, uint32_t destIP, 
	uint16_t sourcePort, uint16_t destPort, uint8_t protocol) 
	: sourceIP(sourceIP), destIP(destIP), sourcePort(sourcePort), 
	  destPort(destPort), protocol(protocol)
{
	queueLen = 0;
	head = tail = NULL;
	next = flowDescriptorReverse = NULL;
	firstOutOfOrderPkt = firstUnscannedPkt = NULL;
}

FlowDescriptorIPv4::~FlowDescriptorIPv4()
{

}

bool
FlowDescriptorIPv4::match(uint32_t srcIP, uint32_t dstIP, uint16_t srcPort,
	uint16_t dstPort, uint8_t prot)
{
	return sourceIP == srcIP && destIP == dstIP && sourcePort == srcPort
		&& destPort == dstPort && protocol == prot;
}

bool
FlowDescriptorIPv4::match(PacketDescriptor *pktDesc)
{
	return sourceIP == pktDesc->srcIP && 
		destIP == pktDesc->destIP && sourcePort == pktDesc->srcPort &&
		destPort == pktDesc->destPort && protocol == pktDesc->protocol;
}

PacketDescriptor *
FlowDescriptorIPv4::releasePktDescriptors(uint8_t numPackets)
{
	int i;
	PacketDescriptor *pktDesc, *headPktDesc;
	for (i = 0, pktDesc = headPktDesc = head; 
			pktDesc != NULL && i < numPackets; 
			i++, pktDesc = pktDesc->next) {
		/*
		printf("releasing %u (%d/%u) pkt\n", pktDesc->packetBufferIndex, i,
			numPackets);
		*/
		if (pktDesc == firstOutOfOrderPkt)
			firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
		if (pktDesc == firstUnscannedPkt)
			firstUnscannedPkt = findNextUnscannedPkt(firstUnscannedPkt); 
		queueLen--;
	}
	if (pktDesc == NULL) {
		head = tail = NULL;
	} else {
		head = pktDesc;
		head->prev = NULL;
		head->flag &= ~PACKET_OUT_OF_ORDER;		
	}
	
	#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
	bool reverse = false;
	if(!isValid(reverse)) {
		if (!reverse)
			printf("releasePktDescriptors1: invalid forward\n");
		else
			printf("releasePktDescriptors1: invalid reverse\n");
		print();
		printReverse();
		assert(false);
	}
	#endif
	return headPktDesc;
}

PacketDescriptor *
FlowDescriptorIPv4::releasePktDescriptors(
	PacketDescriptor *lastPktDesc, bool releaseLastPktDesc)
{
	PacketDescriptor *pktDesc, *headPktDesc;
	for (pktDesc = headPktDesc = head; 
			pktDesc != NULL && pktDesc != lastPktDesc; 
			pktDesc = pktDesc->next) {
		/*
		printf("releasing %u (%d/%u) pkt\n", pktDesc->packetBufferIndex, i,
			numPackets);
		*/
		if (pktDesc == firstOutOfOrderPkt)
			firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
		if (pktDesc == firstUnscannedPkt)
			firstUnscannedPkt = findNextUnscannedPkt(firstUnscannedPkt);
		queueLen--;
	}
	if (pktDesc == NULL) {
		head = tail = NULL;
	} else {
		if (releaseLastPktDesc) {
			if (pktDesc == firstOutOfOrderPkt)
				firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
			if (pktDesc == firstUnscannedPkt)
				firstUnscannedPkt = findNextUnscannedPkt(firstUnscannedPkt);
			queueLen--;
			head = pktDesc->next;
			if (head == NULL)
				tail = NULL;
			else {
				head->prev = NULL;
				head->flag &= ~PACKET_OUT_OF_ORDER;
			}
		}
		else {
			head = pktDesc;
			head->prev = NULL;
			head->flag &= ~PACKET_OUT_OF_ORDER;
		}
	}
	
	#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
	bool reverse = false;
	if(!isValid(reverse)) {
		if (!reverse)
			printf("releasePktDescriptors2: invalid forward\n");
		else
			printf("releasePktDescriptors2: invalid reverse\n");
		print();
		printReverse();
		assert(false);
	}
	//printf("releasePktDescriptors2\n"); print(); printReverse(); //TODO: DEL
	#endif
	return headPktDesc;
}

PacketDescriptor *
FlowDescriptorIPv4::releasePktDescriptors(PacketDescriptor *firstPktDesc,
	PacketDescriptor *lastPktDesc, bool releaseLastPktDesc)
{
	PacketDescriptor *pktDesc, *prevPktDesc;
	/* // commented out for unit tests to work
	#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
	assert(match(firstPktDesc));
	#endif
	*/
	
	if (firstPktDesc == head)
		return releasePktDescriptors(lastPktDesc, releaseLastPktDesc);		

	prevPktDesc = firstPktDesc->prev;
	assert(prevPktDesc != NULL);
	for (pktDesc = firstPktDesc; 
			pktDesc != NULL && pktDesc != lastPktDesc; 
			pktDesc = pktDesc->next) {
		if (pktDesc == firstOutOfOrderPkt)
			firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
		if (pktDesc == firstUnscannedPkt)
			firstUnscannedPkt = findNextUnscannedPkt(firstUnscannedPkt);
		queueLen--;
	}
	if (pktDesc == NULL) {
		tail = prevPktDesc;
		tail->next = NULL;
	} else {
		if (releaseLastPktDesc) {
			if (pktDesc == firstOutOfOrderPkt)
				firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
			if (pktDesc == firstUnscannedPkt)
				firstUnscannedPkt = findNextUnscannedPkt(firstUnscannedPkt);
			queueLen--;
			prevPktDesc->next = pktDesc->next;
			if (pktDesc->next == NULL) {
				tail = prevPktDesc;
			}
			else
				pktDesc->next->prev = prevPktDesc;
		}
		else { 
			prevPktDesc->next = pktDesc;
			pktDesc->prev = prevPktDesc;
		}
		if (prevPktDesc->next != NULL) {
			/* removing this sequence of packets may make the first packet
			 * after the sequence, the first out of order packet. 		*/
			if (firstOutOfOrderPkt == NULL 
					||
					firstOutOfOrderPkt->tcpSeqNum - 
						prevPktDesc->next->tcpSeqNum <
						MAX_TCP_RECV_WINDOW)
				firstOutOfOrderPkt = prevPktDesc->next;
			prevPktDesc->next->flag |= PACKET_OUT_OF_ORDER;
		}
	}
	#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
	bool reverse = false;
	if(!isValid(reverse)) {
		if (!reverse)
			printf("releasePktDescriptors3: invalid forward\n");
		else
			printf("releasePktDescriptors3: invalid reverse\n");
		print();
		printReverse();
		assert(false);
	}
	//printf("releasePktDescriptors3\n"); print(); printReverse(); //TODO: DEL
	#endif
	return firstPktDesc;
}

PacketDescriptor *
FlowDescriptorIPv4::findNextOutOfOrderPkt(PacketDescriptor *pktDesc)
{
	PacketDescriptor *tmpPktDesc = pktDesc->next;
	while (tmpPktDesc != NULL && 
			((tmpPktDesc->flag & PACKET_OUT_OF_ORDER) == 0)) 
		tmpPktDesc = tmpPktDesc->next;
	return tmpPktDesc;	
}

PacketDescriptor *
FlowDescriptorIPv4::findNextUnscannedPkt(PacketDescriptor *pktDesc)
{
	PacketDescriptor *tmpPktDesc = pktDesc->next;
	while (tmpPktDesc != NULL && 
			(tmpPktDesc->flag & PACKET_SCANNED)) 
		tmpPktDesc = tmpPktDesc->next;
	return tmpPktDesc;	
}

void
FlowDescriptorIPv4::print()
{
	PacketDescriptor *pktDesc;
	int count;
	printf("flow: ");
	for (pktDesc = head, count = 0; 
			pktDesc != NULL /*&& 
			count <= RPC_PARSER_GC_THRESH + RPC_PARSER_PACKETS_BATCH_SIZE*/; 
			pktDesc = pktDesc->next, count++)
		printf("%u(%u/%u/%u) -> ", pktDesc->packetBufferIndex, 
			pktDesc->visitCount, pktDesc->flag, pktDesc->refCount.load());
	printf(" count=%d\n", count);
}

void
FlowDescriptorIPv4::printReverse()
{
	PacketDescriptor *pktDesc;
	int count;
	printf("reverse flow: ");
	for (pktDesc = tail, count = 0; 
			pktDesc != NULL /*&&
			count <= RPC_PARSER_GC_THRESH + RPC_PARSER_PACKETS_BATCH_SIZE*/; 
			pktDesc = pktDesc->prev, count++)
		printf("%u(%u/%u/%u) -> ", pktDesc->packetBufferIndex, 
			pktDesc->visitCount, pktDesc->flag, pktDesc->refCount.load());
	printf(" count=%d\n", count);
}

void
FlowDescriptorIPv4::printTcp()
{
	PacketDescriptor *pktDesc;
	int count;
	printf("flow: ");
	for (pktDesc = head, count = 0; 
			pktDesc != NULL /*&& 
			count <= RPC_PARSER_GC_THRESH + RPC_PARSER_PACKETS_BATCH_SIZE*/; 
			pktDesc = pktDesc->next, count++)
		printf("%u(%u/%u/%u/%u) -> ", pktDesc->packetBufferIndex, 
			pktDesc->tcpSeqNum, pktDesc->tcpAckNum, pktDesc->payloadLength,
			pktDesc->flag);
	printf(" count=%d\n", count);
}

FlowDescriptorTcp::FlowDescriptorTcp(uint32_t sourceIP, uint32_t destIP,
	uint16_t sourcePort, uint16_t destPort, uint8_t protocol)
	: FlowDescriptorIPv4(sourceIP, destIP, sourcePort, destPort, protocol)
{
	maxSeqNumSeen = maxAckNumSeen = 0;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	seenPacketCount = outOfOrderCount = emptyAckCount = goodRetransCount = 
		badRetransCount = wrapGoodRetransCount = wrapBadRetransCount =
		staleRetransCount = recoveredOutOfOrderCount = 
		tailDiscardCount = wrapDiscardCount = miscDiscardCount = 
		truncatedPktCount = droppedPktCount = 0;
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
	badRetransDuplicateCount = badRetransNonDuplicateCount = 0;
	#endif
}

FlowDescriptorTcp::~FlowDescriptorTcp()
{

}

bool
FlowDescriptorTcp::insertPacketDescriptor(PacketDescriptor *pktDesc,
	bool &forceFlush)
{
	/* // commented out for unit tests to work
	#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
	assert(match(pktDesc));
	#endif
	*/	
	#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
	pktDesc->print();
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	seenPacketCount++;
	#endif
	if (pktDesc->payloadLength == 0) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
		printf("[empty ack]\n");
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		emptyAckCount++;
		#endif
		return false;
	}
	uint32_t maxSeqNumInPacket = pktDesc->tcpSeqNum + pktDesc->payloadLength;
	if (maxSeqNumInPacket < maxSeqNumSeen) {
		// use TCP window size to detect seq wrap-around
		if (maxSeqNumSeen - maxSeqNumInPacket > MAX_TCP_RECV_WINDOW) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[wrap maxSeqNumSeen=%u maxSeqNumInPacket=%u]\n", 
				maxSeqNumSeen, maxSeqNumInPacket);
			#endif
			if (tail) {
				tail->next = pktDesc;
				if (tail->tcpSeqNum + tail->payloadLength !=
						pktDesc->tcpSeqNum) {
					pktDesc->flag |= PACKET_OUT_OF_ORDER;
					#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
					printf("[out of order]\n");
					#endif
					#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
					outOfOrderCount++;
					#endif
					if (firstOutOfOrderPkt == NULL)
						firstOutOfOrderPkt = pktDesc;
				}
			} else {
				head = pktDesc;
				head->prev = NULL;
			}
			pktDesc->prev = tail;
			pktDesc->next = NULL;
			tail = pktDesc;
			queueLen++;
			maxSeqNumSeen = maxSeqNumInPacket;
			maxAckNumSeen = pktDesc->tcpAckNum;
			forceFlush = true;
			if (firstUnscannedPkt == NULL)
				firstUnscannedPkt = pktDesc;
			return true;
		} else { // a retransmission (may fill the hole)
			return handleRetransmission(pktDesc, maxSeqNumInPacket);
		}
	} else if (maxSeqNumInPacket == maxSeqNumSeen) {
		if (maxAckNumSeen < pktDesc->tcpAckNum) {
			maxAckNumSeen = pktDesc->tcpAckNum;
		} else if (maxAckNumSeen > pktDesc->tcpAckNum + MAX_TCP_RECV_WINDOW) {
			//handle wrap in ack numbers
			maxAckNumSeen = pktDesc->tcpAckNum;
		}
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
		printf("[retrans]\n");
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		badRetransCount++;
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
		if (findDuplicate(pktDesc))
			badRetransDuplicateCount++;
		else
			badRetransNonDuplicateCount++;
		#endif
		return false;
	} else { // maxSeqNumInPacket > maxSeqNumSeen
		if (head && maxSeqNumInPacket - maxSeqNumSeen > MAX_TCP_RECV_WINDOW) {
			if (head->tcpSeqNum < pktDesc->tcpSeqNum && 
					handleRetransmission(pktDesc, maxSeqNumInPacket)) {
				#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
				wrapGoodRetransCount++;
				#endif
				return true;
			}
			// ignore retransmissions during seq wrap
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			wrapBadRetransCount++;
			badRetransCount++;
			#endif
			return false;
		}
		// packet is advancing the TCP congestion window
		if (tail && pktDesc->tcpSeqNum >= tail->tcpSeqNum &&
				pktDesc->tcpSeqNum < tail->tcpSeqNum + tail->payloadLength) {
			// packet overlaps with tail
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("insertPacketDescriptor: STRADDLE tail BEFORE: " 
				"pktDesc->tcpSeqNum+pktDesc->payloadLength:%u+%u "
				"tail->tcpSeqNum+tail->payloadLength:%u+%u\n", 
				pktDesc->tcpSeqNum, pktDesc->payloadLength, 
				tail->tcpSeqNum, tail->payloadLength);
			#endif
			if (pktDesc->tcpSeqNum == tail->tcpSeqNum) {
				// should discard the tail but for now ignore the new packet
				#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
				tailDiscardCount++;
				#endif
				return false;				
			} else if (pktDesc->tcpSeqNum > tail->tcpSeqNum &&
					tail->toParseOffset < (tail->pcapHeader.caplen - 
					(tail->tcpSeqNum + tail->payloadLength - 
					pktDesc->tcpSeqNum))) {
				// truncating the tail
				tail->pcapHeader.caplen -= tail->tcpSeqNum + 
					tail->payloadLength - pktDesc->tcpSeqNum;
				tail->payloadLength = pktDesc->tcpSeqNum - tail->tcpSeqNum;
				tail->flag |= PACKET_TRUNCATED;
				maxSeqNumSeen = pktDesc->tcpSeqNum;
				#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
				printf("insertPacketDescriptor: STRADDLE tail AFTER: " 
					"pktDesc->tcpSeqNum+pktDesc->payloadLength:%u+%u "
					"tail->tcpSeqNum+tail->payloadLength:%u+%u\n", 
					pktDesc->tcpSeqNum, pktDesc->payloadLength, 
					tail->tcpSeqNum, tail->payloadLength);
				#endif
				#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
				truncatedPktCount++;
				#endif
			} else {
				// should discard the tail but for now ignore the new packet
				#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
				tailDiscardCount++;
				#endif
				return false;	
			}
		} else if (tail && 
				(tail->tcpSeqNum - pktDesc->tcpSeqNum < MAX_TCP_RECV_WINDOW)) { 
			//TODO: CHANGE 
			// ignore! new packet straddles multiple packets 
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[retrans wrap] ignored\n");
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			wrapDiscardCount++;
			#endif
			return false;
		}
		if (maxSeqNumSeen != pktDesc->tcpSeqNum) {
			pktDesc->flag |= PACKET_OUT_OF_ORDER;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[out of order]\n");
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			outOfOrderCount++;
			#endif
			if ((firstOutOfOrderPkt != NULL &&
					pktDesc->tcpSeqNum < firstOutOfOrderPkt->tcpSeqNum) ||
					firstOutOfOrderPkt == NULL)
				firstOutOfOrderPkt = pktDesc;
			// TODO: supporting truncated packets
		} else {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[good in order] maxSeqNumSeen=%u\n", maxSeqNumSeen);
			#endif
		}
		if (tail == NULL) {
			pktDesc->flag &= ~PACKET_OUT_OF_ORDER;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			if (!(pktDesc->flag & PACKET_OUT_OF_ORDER))
				printf("[correction -> first pkt in order]\n");
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			if (!maxSeqNumSeen)
				outOfOrderCount--;
			#endif
			pktDesc->prev = pktDesc->next = NULL;
			head = tail = pktDesc;
			firstOutOfOrderPkt = NULL;
		} else {
			tail->next = pktDesc;
			pktDesc->prev = tail;
			pktDesc->next = NULL;
			if (tail->tcpSeqNum + tail->payloadLength != pktDesc->tcpSeqNum) {
				pktDesc->flag |= PACKET_OUT_OF_ORDER;
				#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
				printf("[correction -> out of order (due to removed pkts)]\n");
				/* //TODO: DEL
				printf("tail: "); 
				tail->print();
				printf("pkt: ");
				pktDesc->print();
				*/			
				#endif
			}
			tail = pktDesc;
			//print(); printReverse(); //TODO: DEL
		}
		queueLen++;
		maxSeqNumSeen = maxSeqNumInPacket;
		maxAckNumSeen = pktDesc->tcpAckNum;
		if (firstUnscannedPkt == NULL)
			firstUnscannedPkt = pktDesc;
	}
	return true;
}

bool
FlowDescriptorTcp::handleRetransmission(PacketDescriptor *pktDesc,
	uint32_t maxSeqNumInPacket)
{
	if (head != NULL && maxSeqNumInPacket <= head->tcpSeqNum &&
			head->tcpSeqNum - maxSeqNumInPacket < MAX_TCP_RECV_WINDOW &&
			head->tcpSeqNum - pktDesc->tcpSeqNum < FLOW_DESC_TCP_HEAD_WINDOW) {
		//TODO: handle wraps?
		if (pktDesc->tcpSeqNum + pktDesc->payloadLength == head->tcpSeqNum)
			;
		else if (pktDesc->tcpSeqNum + pktDesc->payloadLength < head->tcpSeqNum) {
			head->flag |= PACKET_OUT_OF_ORDER;
			firstOutOfOrderPkt = head;
		} else {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)		
			badRetransCount++;
			miscDiscardCount++;
			#endif
			return false;
		}
		pktDesc->next = head;
		pktDesc->prev = NULL;
		pktDesc->visitCount = head->visitCount;
		head->prev = pktDesc;
		head = pktDesc;
		queueLen++;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		goodRetransCount++;
		if (head && pktDesc->pcapHeader.ts.tv_sec == 
				pktDesc->next->pcapHeader.ts.tv_sec &&
				pktDesc->pcapHeader.ts.tv_usec == 
				pktDesc->next->pcapHeader.ts.tv_usec) {
			recoveredOutOfOrderCount++;
		}
		#endif
		firstUnscannedPkt = head;
		return true;
	} else if (head != NULL && 
			head->tcpSeqNum - maxSeqNumInPacket < MAX_TCP_RECV_WINDOW &&
			maxSeqNumInPacket <= head->tcpSeqNum) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		badRetransCount++;
		staleRetransCount++;
		#endif
		return false;
	}
	if (firstOutOfOrderPkt == NULL) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
		printf("[retrans]\n");
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		badRetransCount++;
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
		if (findDuplicate(pktDesc))
			badRetransDuplicateCount++;
		else
			badRetransNonDuplicateCount++;
		#endif
		return false;
	}
	if (insertPktInHole(pktDesc, firstOutOfOrderPkt)) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
		printf("[out of order/fills hole]\n");
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		goodRetransCount++;
		if (pktDesc->pcapHeader.ts.tv_sec == 
				pktDesc->next->pcapHeader.ts.tv_sec &&
				pktDesc->pcapHeader.ts.tv_usec == 
				pktDesc->next->pcapHeader.ts.tv_usec) {
			recoveredOutOfOrderCount++;
		}
		#endif
		queueLen++;
		if (firstUnscannedPkt == NULL ||
				firstUnscannedPkt->tcpSeqNum - pktDesc->tcpSeqNum < 
					MAX_TCP_RECV_WINDOW)
			firstUnscannedPkt = pktDesc;
		return true;
	}
	// may fill some hole but not the first hole
	PacketDescriptor *nextHolePktDesc = firstOutOfOrderPkt;
	do {
		nextHolePktDesc = findNextOutOfOrderPkt(nextHolePktDesc);
		if (nextHolePktDesc == NULL) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[retrans]\n");
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			badRetransCount++;
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
			if (findDuplicate(pktDesc))
				badRetransDuplicateCount++;
			else
				badRetransNonDuplicateCount++;
			#endif
			return false;
		}
		if (insertPktInHole(pktDesc, nextHolePktDesc)) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("[out of order/fills hole]\n");
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			goodRetransCount++;
			if (pktDesc->pcapHeader.ts.tv_sec == 
					pktDesc->next->pcapHeader.ts.tv_sec &&
					pktDesc->pcapHeader.ts.tv_usec == 
					pktDesc->next->pcapHeader.ts.tv_usec) {
				recoveredOutOfOrderCount++;
			}
			#endif
			queueLen++;
			if (firstUnscannedPkt == NULL ||
					firstUnscannedPkt->tcpSeqNum - pktDesc->tcpSeqNum 
						< MAX_TCP_RECV_WINDOW)
				firstUnscannedPkt = pktDesc;
			return true;
		}
	} while (nextHolePktDesc->tcpSeqNum <= pktDesc->tcpSeqNum ||
			(nextHolePktDesc->tcpSeqNum > pktDesc->tcpSeqNum && 
			nextHolePktDesc->tcpSeqNum - pktDesc->tcpSeqNum > 
			MAX_TCP_RECV_WINDOW)); 
	#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
	printf("[retrans]\n");
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	badRetransCount++;	
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
	if (findDuplicate(pktDesc))
		badRetransDuplicateCount++;
	else
		badRetransNonDuplicateCount++;
	#endif
	return false;
}

bool
FlowDescriptorTcp::insertPktInHole(PacketDescriptor *pktDesc, 
	PacketDescriptor *outOfOrderPktDesc)
{
	PacketDescriptor *prevPktDesc = outOfOrderPktDesc->prev;
	if (prevPktDesc == NULL)
		return false;
	if (pktDesc->tcpSeqNum + pktDesc->payloadLength 
			<= outOfOrderPktDesc->tcpSeqNum &&
			prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength
			<= pktDesc->tcpSeqNum) 
		; // good packet
	else if (prevPktDesc->tcpSeqNum < pktDesc->tcpSeqNum &&
				pktDesc->tcpSeqNum < outOfOrderPktDesc->tcpSeqNum) {
		if (pktDesc->tcpSeqNum + pktDesc->payloadLength > 
				outOfOrderPktDesc->tcpSeqNum) {
			// good packet; packet needs to be truncated
			pktDesc->pcapHeader.caplen -= pktDesc->tcpSeqNum + 
				pktDesc->payloadLength - outOfOrderPktDesc->tcpSeqNum;
			pktDesc->payloadLength = outOfOrderPktDesc->tcpSeqNum - 
				pktDesc->tcpSeqNum;
			pktDesc->flag |= PACKET_TRUNCATED;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("insertPktInHole: STRADDLE hole " 
				"pktDesc->tcpSeqNum+pktDesc->payloadLength:%u+%u "
				"outOfOrderPktDesc->tcpSeqNum:%u\n", pktDesc->tcpSeqNum, 
				pktDesc->payloadLength, outOfOrderPktDesc->tcpSeqNum);
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			truncatedPktCount++;
			#endif
		}
		if (pktDesc->tcpSeqNum < prevPktDesc->tcpSeqNum + 
				prevPktDesc->payloadLength &&
				prevPktDesc->toParseOffset < (prevPktDesc->pcapHeader.caplen
				- (prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength - 
				pktDesc->tcpSeqNum))) {
			// good packet; prev packet needs to be truncated
			prevPktDesc->pcapHeader.caplen -= prevPktDesc->tcpSeqNum + 
				prevPktDesc->payloadLength - pktDesc->tcpSeqNum;
			prevPktDesc->payloadLength = pktDesc->tcpSeqNum - 
				prevPktDesc->tcpSeqNum;
			prevPktDesc->flag |= PACKET_TRUNCATED;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			printf("insertPktInHole: STRADDLE hole " 
				"prevPktDesc->tcpSeqNum+prevPktDesc->payloadLength:%u+%u "
				"pktDesc->tcpSeqNum:%u\n", prevPktDesc->tcpSeqNum, 
				prevPktDesc->payloadLength, pktDesc->tcpSeqNum);
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			truncatedPktCount++;
			#endif
		} else if (pktDesc->tcpSeqNum < prevPktDesc->tcpSeqNum + 
				prevPktDesc->payloadLength &&
				prevPktDesc->toParseOffset >= (prevPktDesc->pcapHeader.caplen
				- (prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength - 
				pktDesc->tcpSeqNum))) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			miscDiscardCount++;
			#endif
			return false;
		}
	}
	else if (prevPktDesc->tcpSeqNum > outOfOrderPktDesc->tcpSeqNum  && 
			(prevPktDesc->tcpSeqNum - outOfOrderPktDesc->tcpSeqNum >
			 MAX_TCP_RECV_WINDOW)) { // wrapped seq number
		/*
		if (prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength 
				< pktDesc->tcpSeqNum) { // pkt starts before wrap
			if (pktDesc->tcpSeqNum + pktDesc->payloadLength > 
					outOfOrderPktDesc->tcpSeqNum + MAX_TCP_RECV_WINDOW)
				;
			else 
				return false;
		} else if (prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength 
				> pktDesc->tcpSeqNum + MAX_TCP_RECV_WINDOW) {
			if (pktDesc->tcpSeqNum + pktDesc->payloadLength < outOfOrderPktDesc->tcpSeqNum)
				;
			else 
				return false;
		else 
		*/
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		wrapDiscardCount++;
		#endif
		return false;
	} else
		return false;
	
	prevPktDesc->next = pktDesc;
	pktDesc->prev = prevPktDesc;
	outOfOrderPktDesc->prev = pktDesc;
	pktDesc->next = outOfOrderPktDesc;

	// guaranteeing sequentiality of visitCount for packets (GC)
	pktDesc->visitCount = outOfOrderPktDesc->visitCount;

	if (prevPktDesc->tcpSeqNum + prevPktDesc->payloadLength !=
			pktDesc->tcpSeqNum) {
		pktDesc->flag |= PACKET_OUT_OF_ORDER;
		if (firstOutOfOrderPkt == outOfOrderPktDesc)
			firstOutOfOrderPkt = pktDesc;
	}  
	if (pktDesc->tcpSeqNum + pktDesc->payloadLength == 
			outOfOrderPktDesc->tcpSeqNum) { 
		outOfOrderPktDesc->flag ^= PACKET_OUT_OF_ORDER;
		if (firstOutOfOrderPkt == outOfOrderPktDesc)
			firstOutOfOrderPkt = findNextOutOfOrderPkt(firstOutOfOrderPkt);
	} 
	return true;	
}

void FlowDescriptorTcp::printStats()
{
	printf("[");
	printIpAddress(sourceIP);
	printf(":%u -> ", sourcePort);
	printIpAddress(destIP);
	printf(":%u] ", destPort);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	printf("seenPacketCount:%lu outOfOrderCount:%lu emptyAckCount:%lu "
		"goodRetransCount:%lu badRetransCount:%lu wrapGoodRetransCount:%lu "
		"wrapBadRetransCount:%lu staleRetransCount:%lu "
		"recoveredOutOfOrderCount:%lu "
		"tailDiscardCount:%lu wrapDiscardCount:%lu miscDiscardCount:%lu "
		"truncatedPktCount:%lu droppedPktCount:%lu ", 
		seenPacketCount, outOfOrderCount, emptyAckCount, goodRetransCount, 
		badRetransCount, wrapGoodRetransCount, wrapBadRetransCount, 
		staleRetransCount, recoveredOutOfOrderCount,
		tailDiscardCount, wrapDiscardCount, miscDiscardCount,
		truncatedPktCount, droppedPktCount);
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK_STAT)
	printf("badRetransDuplicateCount:%lu badRetransNonDuplicateCount:%lu ",
		badRetransDuplicateCount, badRetransNonDuplicateCount);
	#endif
	//printf("\n");
}

bool FlowDescriptorTcp::findDuplicate(PacketDescriptor *pktDesc)
{
	PacketDescriptor *tmpPktDesc = head;
	while (tmpPktDesc && 
			(tmpPktDesc->tcpSeqNum < pktDesc->tcpSeqNum ||
			(tmpPktDesc->tcpSeqNum - pktDesc->tcpSeqNum > MAX_TCP_RECV_WINDOW))) {
		tmpPktDesc = tmpPktDesc->next;
	}
	if (tmpPktDesc == NULL)
		return false;
	if (tmpPktDesc->tcpSeqNum == pktDesc->tcpSeqNum 
			&& tmpPktDesc->payloadLength == pktDesc->payloadLength)
		return true;
	return false;
}

bool
FlowDescriptorTcp::isValid(bool &reverse)
{
	PacketDescriptor *pktDesc1 = head, *pktDesc2 = NULL;
	unsigned count = 0;
	if (head == NULL)
		return (tail == NULL);
	if (head->prev != NULL || tail->next != NULL) {
		printf("head->prev:%p tail->next:%p\n", head->prev, tail->next);
		return false;
	}
	while (pktDesc1 != NULL) {
		count++;
		if (pktDesc2 == NULL) {
			if ((pktDesc1->flag & PACKET_OUT_OF_ORDER) != 0) {
				printf("forward: head is out of order!\n");
				return false;
			}
		} else {
			if (pktDesc2->tcpSeqNum + pktDesc2->payloadLength > 
					pktDesc1->tcpSeqNum) {
				if (pktDesc2->tcpSeqNum + pktDesc2->payloadLength 
						- pktDesc1->tcpSeqNum < MAX_TCP_RECV_WINDOW) {
					printf("forward: out of order seq number: "
						" pktDesc2->packetBufferIndex:%u pktDesc2->tcpSeqNum:%u"
						" pktDesc2->payloadLength:%u pktDesc1->tcpSeqNum:%u\n", 
						pktDesc2->packetBufferIndex, pktDesc2->tcpSeqNum, 
						pktDesc2->payloadLength, pktDesc1->tcpSeqNum);
					return false;
				}
			}
			if (pktDesc2->tcpSeqNum + pktDesc2->payloadLength != 
					pktDesc1->tcpSeqNum &&  
					!(pktDesc1->flag & PACKET_OUT_OF_ORDER)) {
				printf("forward: out of order flag not set:"
					" pktDesc2->tcpSeqNum:%u pktDesc2->payloadLength:%u"
					" pktDesc1->tcpSeqNum:%u\n", pktDesc2->tcpSeqNum,
					pktDesc2->payloadLength, pktDesc1->tcpSeqNum);
				return false;
			}
			if (pktDesc1->prev != pktDesc2) {
				printf("reverse: pktDesc1->prev:%p pktDesc2:%p\n", 
					pktDesc1->prev, pktDesc2);
				reverse = true;
				return false;
			}
		}			
		pktDesc2 = pktDesc1;
		pktDesc1 = pktDesc1->next;
	}
	if (pktDesc1 == NULL && (pktDesc2 != tail || count != queueLen)) {
		printf("forward: head:%p tail:%p count:%u queueLen:%u\n", 
			head, tail, count, queueLen);
		reverse = false;
		return false;
	}
	return true;
}


