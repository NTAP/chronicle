// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include <netinet/in.h>
#include "gtest/gtest.h"
#include "ChronicleProcessRequest.h"
#include "FlowDescriptor.h"
#include "PacketBuffer.h"

TEST(FlowDescriptor, testTcpRetrans) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = 1000;
	pktDesc1->tcpAckNum = 1;
	pktDesc1->payloadLength = 100;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	// creating a hole of 100B
	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 1200;
	pktDesc2->tcpAckNum = 1;
	pktDesc2->payloadLength = 100;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));

	// testing useless retransmissions
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));

	// testing a useful retransmission / out of order packet
	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 1100;
	pktDesc3->tcpAckNum = 1;
	pktDesc3->payloadLength = 100;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));

	// testing another useless retransmission
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));

	delete pktDesc1; delete pktDesc2; delete pktDesc3;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpLossRecovery) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);
	PacketDescriptor *pktDesc = new PacketDescriptor[11];
	PacketDescriptor *stalePktDesc = new PacketDescriptor[4];

	pktDesc[1].packetBufferIndex = 1;
	pktDesc[1].flag = 0;
	pktDesc[1].tcpSeqNum = 1000;
	pktDesc[1].tcpAckNum = 1;
	pktDesc[1].payloadLength = 100;
	pktDesc[1].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[1], forceFlush));

	// creating the first 100B hole
	pktDesc[2].packetBufferIndex = 2;
	pktDesc[2].flag = 0;
	pktDesc[2].tcpSeqNum = 1200;
	pktDesc[2].tcpAckNum = 1;
	pktDesc[2].payloadLength = 100;
	pktDesc[2].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[2], forceFlush));

	// creating the second 100B hole
	pktDesc[3].packetBufferIndex = 3;
	pktDesc[3].flag = 0;
	pktDesc[3].tcpSeqNum = 1400;
	pktDesc[3].tcpAckNum = 1;
	pktDesc[3].payloadLength = 100;
	pktDesc[3].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[3], forceFlush));

	// retransmitting a stale packet
	stalePktDesc[1].packetBufferIndex = 1001;
	stalePktDesc[1].flag = 0;
	stalePktDesc[1].tcpSeqNum = 900;
	stalePktDesc[1].tcpAckNum = 1;
	stalePktDesc[1].payloadLength = 100;
	stalePktDesc[1].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&stalePktDesc[1], 
		forceFlush));

	// creating the third 100B hole
	pktDesc[4].packetBufferIndex = 4;
	pktDesc[4].flag = 0;
	pktDesc[4].tcpSeqNum = 1600;
	pktDesc[4].tcpAckNum = 1;
	pktDesc[4].payloadLength = 100;
	pktDesc[4].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[4], forceFlush));

	// recovering the first hole completely
	pktDesc[5].packetBufferIndex = 5;
	pktDesc[5].flag = 0;
	pktDesc[5].tcpSeqNum = 1100;
	pktDesc[5].tcpAckNum = 1;
	pktDesc[5].payloadLength = 100;
	pktDesc[5].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[5], forceFlush));

	// retransmitting a stale packet
	stalePktDesc[2].packetBufferIndex = 1002;
	stalePktDesc[2].flag = 0;
	stalePktDesc[2].tcpSeqNum = 1100;
	stalePktDesc[2].tcpAckNum = 1;
	stalePktDesc[2].payloadLength = 100;
	stalePktDesc[2].pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(&stalePktDesc[2], 
		forceFlush));

	// recovering the third hole partially
	pktDesc[6].packetBufferIndex = 6;
	pktDesc[6].flag = 0;
	pktDesc[6].tcpSeqNum = 1533;
	pktDesc[6].tcpAckNum = 1;
	pktDesc[6].payloadLength = 33;
	pktDesc[6].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[6], forceFlush));

	// recovering the second hole partially
	pktDesc[7].packetBufferIndex = 7;
	pktDesc[7].flag = 0;
	pktDesc[7].tcpSeqNum = 1350;
	pktDesc[7].tcpAckNum = 1;
	pktDesc[7].payloadLength = 50;
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[7], forceFlush));

	// recovering the second hole completely
	pktDesc[8].packetBufferIndex = 8;
	pktDesc[8].flag = 0;
	pktDesc[8].tcpSeqNum = 1300;
	pktDesc[8].tcpAckNum = 1;
	pktDesc[8].payloadLength = 50;
	pktDesc[8].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[8], forceFlush));

	// retransmitting a stale packet
	stalePktDesc[3].packetBufferIndex = 1003;
	stalePktDesc[3].flag = 0;
	stalePktDesc[3].tcpSeqNum = 1300;
	stalePktDesc[3].tcpAckNum = 1;
	stalePktDesc[3].payloadLength = 100;
	stalePktDesc[3].pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(&stalePktDesc[3], 
		forceFlush));

	// recovering the third hole completely
	pktDesc[9].packetBufferIndex = 9;
	pktDesc[9].flag = 0;
	pktDesc[9].tcpSeqNum = 1566;
	pktDesc[9].tcpAckNum = 1;
	pktDesc[9].payloadLength = 34;
	pktDesc[9].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[9], forceFlush));
	pktDesc[10].packetBufferIndex = 10;
	pktDesc[10].flag = 0;
	pktDesc[10].tcpSeqNum = 1500;
	pktDesc[10].tcpAckNum = 1;
	pktDesc[10].payloadLength = 33;
	pktDesc[10].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[10], forceFlush));
	
	delete[] pktDesc;
	delete[] stalePktDesc;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpNoLossRecovery) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);
	PacketDescriptor *pktDesc = new PacketDescriptor[11];

	pktDesc[1].packetBufferIndex = 1;
	pktDesc[1].flag = 0;
	pktDesc[1].tcpSeqNum = 1000;
	pktDesc[1].tcpAckNum = 1;
	pktDesc[1].payloadLength = 100;
	pktDesc[1].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[1], forceFlush));

	// creating the first 100B hole
	pktDesc[2].packetBufferIndex = 2;
	pktDesc[2].flag = 0;
	pktDesc[2].tcpSeqNum = 1200;
	pktDesc[2].tcpAckNum = 1;
	pktDesc[2].payloadLength = 100;
	pktDesc[2].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[2], forceFlush));

	// creating the second 100B hole
	pktDesc[3].packetBufferIndex = 3;
	pktDesc[3].flag = 0;
	pktDesc[3].tcpSeqNum = 1400;
	pktDesc[3].tcpAckNum = 1;
	pktDesc[3].payloadLength = 100;
	pktDesc[3].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[3], forceFlush));

	// creating the third 100B hole
	pktDesc[4].packetBufferIndex = 4;
	pktDesc[4].flag = 0;
	pktDesc[4].tcpSeqNum = 1600;
	pktDesc[4].tcpAckNum = 1;
	pktDesc[4].payloadLength = 100;
	pktDesc[4].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[4], forceFlush));

	// not recovering the first hole
	flowDesc->releasePktDescriptors(2);
	ASSERT_EQ(flowDesc->firstOutOfOrderPkt, &pktDesc[3]);

	// not recovering the second hole
	pktDesc[5].packetBufferIndex = 5;
	pktDesc[5].flag = 0;
	pktDesc[5].tcpSeqNum = 1350;
	pktDesc[5].tcpAckNum = 1;
	pktDesc[5].payloadLength = 50;
	pktDesc[5].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[5], forceFlush));

	// recovering the third hole completely
	pktDesc[6].packetBufferIndex = 6;
	pktDesc[6].flag = 0;
	pktDesc[6].tcpSeqNum = 1500;
	pktDesc[6].tcpAckNum = 1;
	pktDesc[6].payloadLength = 100;
	pktDesc[6].pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(&pktDesc[6], forceFlush));
	ASSERT_EQ(pktDesc[3].next, &pktDesc[6]);
	ASSERT_EQ(pktDesc[6].next, &pktDesc[4]);

	delete[] pktDesc;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpSeqWrapInOrder) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = ((1UL << 32) - 50);
	pktDesc1->tcpAckNum = 1;
	pktDesc1->payloadLength = 100;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 50;
	pktDesc2->tcpAckNum = 1;
	pktDesc2->payloadLength = 100;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));
	ASSERT_EQ(pktDesc2->flag & PACKET_OUT_OF_ORDER, 0);
	ASSERT_EQ(pktDesc1->next, pktDesc2);

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 150;
	pktDesc3->tcpAckNum = 1;
	pktDesc3->payloadLength = 100;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));
	ASSERT_EQ(pktDesc2->next, pktDesc3);
	ASSERT_EQ(pktDesc2->flag & PACKET_OUT_OF_ORDER, 0);

	delete pktDesc1; delete pktDesc2; delete pktDesc3;
	delete flowDesc;
}

TEST(FlowDescriptor, testHoleRecoveryAfterTcpSeqWrap) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	// hole is after TCP sequence number has wrapped
	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = (1UL << 32) - 50;
	pktDesc1->tcpAckNum = 1;
	pktDesc1->payloadLength = 100;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 100;
	pktDesc2->tcpAckNum = 1;
	pktDesc2->payloadLength = 100;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));
	ASSERT_EQ(pktDesc2->flag & PACKET_OUT_OF_ORDER, PACKET_OUT_OF_ORDER);
	ASSERT_EQ(pktDesc1->next, pktDesc2);

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 50;
	pktDesc3->tcpAckNum = 1;
	pktDesc3->payloadLength = 50;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));
	ASSERT_EQ(pktDesc3->flag & PACKET_OUT_OF_ORDER, 0);
	ASSERT_EQ(pktDesc1->next, pktDesc3);
	ASSERT_EQ(pktDesc3->next, pktDesc2);
	
	delete pktDesc1; delete pktDesc2; delete pktDesc3;
	delete flowDesc;
}

TEST(FlowDescriptor, testHoleRecoveryDuringTcpSeqWrap) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	// hole is before the sequence number wrap
	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = (1UL << 32) - 50;
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 25;
	pktDesc2->payloadLength = 100;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));
	ASSERT_EQ(pktDesc2->flag & PACKET_OUT_OF_ORDER, PACKET_OUT_OF_ORDER);
	ASSERT_EQ(pktDesc1->next, pktDesc2);

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = (1UL << 32) - 25;
	pktDesc3->payloadLength = 50;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));
	ASSERT_EQ(pktDesc3->flag & PACKET_OUT_OF_ORDER, 0);
	ASSERT_EQ(pktDesc1->next, pktDesc3);
	ASSERT_EQ(pktDesc3->next, pktDesc2);

	delete pktDesc1; delete pktDesc2; delete pktDesc3;
	delete flowDesc;
}

TEST(FlowDescriptor, testMultiHoleRecoveryDuringTcpSeqWrap) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = (1UL << 32) - 100;
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	// one hole is before the sequence number wrap
	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = (1UL << 32) - 50;
	pktDesc2->payloadLength = 100;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));
	
	// one hole is after the sequence number wrap
	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 75;
	pktDesc3->payloadLength = 100;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));

	// recovering the second hole first
	PacketDescriptor *pktDesc4 = new PacketDescriptor();
	pktDesc4->packetBufferIndex = 4;
	pktDesc4->flag = 0;
	pktDesc4->tcpSeqNum = (1UL << 32) - 75;
	pktDesc4->payloadLength = 25;
	pktDesc4->pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc4, forceFlush));
	
	// recovering the first hole next
	PacketDescriptor *pktDesc5 = new PacketDescriptor();
	pktDesc5->packetBufferIndex = 5;
	pktDesc5->flag = 0;
	pktDesc5->tcpSeqNum = 50;
	pktDesc5->payloadLength = 25;
	pktDesc5->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc5, forceFlush));	

	delete pktDesc1; delete pktDesc2; delete pktDesc3, delete pktDesc4;
	delete pktDesc5;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpSeqStraddleTail) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->pcapHeader.caplen = 50;
	pktDesc1->toParseOffset = 25;
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = 100;
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->pcapHeader.caplen = 60;
	pktDesc2->toParseOffset = 25;
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 100;
	pktDesc2->payloadLength = 35;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->pcapHeader.caplen = 105;
	pktDesc3->toParseOffset = 25;
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 50;
	pktDesc3->payloadLength = 80;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));

	PacketDescriptor *pktDesc4 = new PacketDescriptor();
	pktDesc4->pcapHeader.caplen = 50;
	pktDesc4->toParseOffset = 25;
	pktDesc4->packetBufferIndex = 4;
	pktDesc4->flag = 0;
	pktDesc4->tcpSeqNum = 110;
	pktDesc4->payloadLength = 25;
	pktDesc4->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc4, forceFlush));

	ASSERT_TRUE(pktDesc1->next == pktDesc4);
	ASSERT_TRUE(pktDesc4->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == PACKET_TRUNCATED && pktDesc4->flag == 0);

	delete pktDesc1; delete pktDesc2; delete pktDesc3; delete pktDesc4;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpSeqStraddleHole1) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = 100;
	pktDesc1->pcapHeader.caplen = 50;
	pktDesc1->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 150;
	pktDesc2->pcapHeader.caplen = 50; 
	pktDesc2->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc2->payloadLength = 25;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 140;
	pktDesc3->pcapHeader.caplen = 50;
	pktDesc3->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc3->payloadLength = 25;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));
	ASSERT_TRUE(pktDesc1->next == pktDesc3);
	ASSERT_TRUE(pktDesc3->next == pktDesc2);
	ASSERT_TRUE(pktDesc2->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == 0 
		&& pktDesc2->flag == 0
		&& pktDesc3->flag == (PACKET_OUT_OF_ORDER | PACKET_TRUNCATED));
	ASSERT_TRUE(flowDesc->firstOutOfOrderPkt == pktDesc3);
	ASSERT_TRUE(pktDesc3->pcapHeader.caplen == 35);
	ASSERT_TRUE(pktDesc3->payloadLength == 10);

	PacketDescriptor *pktDesc4 = new PacketDescriptor();
	pktDesc4->packetBufferIndex = 4;
	pktDesc4->flag = 0;
	pktDesc4->tcpSeqNum = 110;
	pktDesc4->pcapHeader.caplen = 50; 
	pktDesc4->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc4->payloadLength = 25;
	pktDesc4->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc4, forceFlush));
	ASSERT_TRUE(pktDesc1->next == pktDesc4);
	ASSERT_TRUE(pktDesc4->next == pktDesc3);
	ASSERT_TRUE(pktDesc3->next == pktDesc2);
	ASSERT_TRUE(pktDesc2->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == PACKET_TRUNCATED 
		&& pktDesc2->flag == 0
		&& pktDesc3->flag == (PACKET_OUT_OF_ORDER | PACKET_TRUNCATED)
		&& pktDesc4->flag == 0);
	ASSERT_TRUE(flowDesc->firstOutOfOrderPkt == pktDesc3);
	ASSERT_TRUE(pktDesc1->pcapHeader.caplen == 35);
	ASSERT_TRUE(pktDesc1->payloadLength == 10);

	PacketDescriptor *pktDesc5 = new PacketDescriptor();
	pktDesc5->packetBufferIndex = 5;
	pktDesc5->flag = 0;
	pktDesc5->tcpSeqNum = 135;
	pktDesc5->pcapHeader.caplen = 30;
	pktDesc5->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc5->payloadLength = 5;
	pktDesc5->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc5, forceFlush));
	ASSERT_TRUE(pktDesc1->next == pktDesc4);
	ASSERT_TRUE(pktDesc4->next == pktDesc5);
	ASSERT_TRUE(pktDesc5->next == pktDesc3);
	ASSERT_TRUE(pktDesc3->next == pktDesc2);
	ASSERT_TRUE(pktDesc2->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == PACKET_TRUNCATED 
		&& pktDesc2->flag == 0
		&& pktDesc3->flag == PACKET_TRUNCATED
		&& pktDesc4->flag == 0
		&& pktDesc5->flag == 0);
	ASSERT_TRUE(flowDesc->firstOutOfOrderPkt == NULL);

	delete pktDesc1; delete pktDesc2; delete pktDesc3; delete pktDesc4;
	delete pktDesc5;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpSeqStraddleHole2) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = 100;
	pktDesc1->pcapHeader.caplen = 50; 
	pktDesc1->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 150;
	pktDesc2->pcapHeader.caplen = 50;
	pktDesc2->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc2->payloadLength = 25;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = 120;
	pktDesc3->pcapHeader.caplen = 65;
	pktDesc3->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc3->payloadLength = 40;
	pktDesc3->pcapHeader.ts = {0, 0};
	pktDesc1->toParseOffset = 45;
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));
	pktDesc1->toParseOffset = 25;

	PacketDescriptor *pktDesc4 = new PacketDescriptor();
	pktDesc4->packetBufferIndex = 3;
	pktDesc4->flag = 0;
	pktDesc4->tcpSeqNum = 120;
	pktDesc4->pcapHeader.caplen = 65;
	pktDesc4->toParseOffset = 25;	// TCP header starts at offset 25
	pktDesc4->payloadLength = 40;
	pktDesc4->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc4, forceFlush));
	ASSERT_TRUE(pktDesc1->next == pktDesc4);
	ASSERT_TRUE(pktDesc4->next == pktDesc2);
	ASSERT_TRUE(pktDesc2->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == PACKET_TRUNCATED 
		&& pktDesc2->flag == 0
		&& pktDesc4->flag == PACKET_TRUNCATED);
	ASSERT_TRUE(flowDesc->firstOutOfOrderPkt == NULL);
	ASSERT_TRUE(pktDesc1->pcapHeader.caplen == 45);
	ASSERT_TRUE(pktDesc1->payloadLength == 20);
	ASSERT_TRUE(pktDesc4->pcapHeader.caplen == 55);
	ASSERT_TRUE(pktDesc4->payloadLength == 30);
	
	delete pktDesc1; delete pktDesc2; delete pktDesc3; delete pktDesc4;
	delete flowDesc;
}

TEST(FlowDescriptor, testTcpSeqStraddleHoleWrap) {
	unsigned srcIP = 0x0d0b0c0d, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool forceFlush = false;

	FlowDescriptorIPv4 *flowDesc = new FlowDescriptorTcp(srcIP, destIP, srcPort,
		destPort, IPPROTO_TCP);

	PacketDescriptor *pktDesc1 = new PacketDescriptor();
	pktDesc1->packetBufferIndex = 1;
	pktDesc1->flag = 0;
	pktDesc1->tcpSeqNum = ((1UL << 32) - 50);
	pktDesc1->payloadLength = 25;
	pktDesc1->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc1, forceFlush));

	PacketDescriptor *pktDesc2 = new PacketDescriptor();
	pktDesc2->packetBufferIndex = 2;
	pktDesc2->flag = 0;
	pktDesc2->tcpSeqNum = 25;
	pktDesc2->payloadLength = 50;
	pktDesc2->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc2, forceFlush));
	ASSERT_TRUE(pktDesc2->next == NULL);

	PacketDescriptor *pktDesc3 = new PacketDescriptor();
	pktDesc3->packetBufferIndex = 3;
	pktDesc3->flag = 0;
	pktDesc3->tcpSeqNum = ((1UL << 32) - 25);
	pktDesc3->payloadLength = 100;
	pktDesc3->pcapHeader.ts = {0, 0};
	ASSERT_FALSE(flowDesc->insertPacketDescriptor(pktDesc3, forceFlush));

	PacketDescriptor *pktDesc4 = new PacketDescriptor();
	pktDesc4->packetBufferIndex = 4;
	pktDesc4->flag = 0;
	pktDesc4->tcpSeqNum = ((1UL << 32) - 15);
	pktDesc4->payloadLength = 40;
	pktDesc4->pcapHeader.ts = {0, 0};
	ASSERT_TRUE(flowDesc->insertPacketDescriptor(pktDesc4, forceFlush));

	ASSERT_TRUE(pktDesc1->next == pktDesc4);
	ASSERT_TRUE(pktDesc4->next == pktDesc2);
	ASSERT_TRUE(pktDesc2->next == NULL);
	ASSERT_TRUE(pktDesc1->flag == 0 
		&& pktDesc2->flag == 0
		&& pktDesc4->flag == PACKET_OUT_OF_ORDER);

	delete pktDesc1; delete pktDesc2; delete pktDesc3; delete pktDesc4;
	delete flowDesc;
}


