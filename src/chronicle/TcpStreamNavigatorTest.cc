// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include <cstring>
#include <errno.h>
#include <arpa/inet.h>
#include "gtest/gtest.h"
#include "ChronicleProcessRequest.h"
#include "PcapPacketBufferPool.h"
#include "TcpStreamNavigator.h"
#include "RpcParser.h"
#include "NfsParser.h"

TEST(TcpStreamNavigator, singlePacketTest) {
	PcapPacketBufferPool *poolUser;
	poolUser = PcapPacketBufferPool::registerBufferPool(1);
	PacketDescriptor *pktDesc = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);

	// constructing a fake NFS PDU
	uint32_t pduLen = 1000, xid = 0x0a0b0c0d, progVersion = NFS3_VERSION,
		progProc = NFS3PROC_READ;
	uint16_t progHeaderOffset = 100, headerLen = 100;
	uint8_t msgType = RPC_REPLY;
	pktDesc->pcapHeader.caplen = progHeaderOffset + pduLen - 1;
	PduDescriptor *pduDesc = new NfsV3PduDescriptor(pktDesc, pktDesc,
		pduLen, xid, progVersion, progProc, progHeaderOffset - 1, 
		progHeaderOffset, 0, msgType);
	pduDesc->lastPktDesc = pktDesc;
	pduDesc->rpcProgramEndOffset = pktDesc->pcapHeader.caplen - 1;
	
	// populating the RPC header
	unsigned char *ethFrame = pktDesc->getEthFrameAddress();
	uint32_t uint32, i;
	for (i = 0; i < headerLen; i += 4) { 
		uint32 = htonl(i);	
		memcpy(&ethFrame[progHeaderOffset + i], &uint32, sizeof(uint32));
	}

	// populating the RPC payload (partially)
	char arr[] = "TcpStreamNavigatorTest";
	memcpy(&ethFrame[progHeaderOffset + headerLen], arr, strlen(arr));
	ethFrame[pduDesc->rpcProgramEndOffset] = '@';

	// Parsing RPC header
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	EXPECT_TRUE(streamNavigator->init(pduDesc, progHeaderOffset));
	for (i = 0; i < headerLen; i += 4) { 
		EXPECT_TRUE(streamNavigator->getUint32Pdu(&uint32));
		EXPECT_EQ(uint32, i);
	}

	// Parsing RPC payload
	unsigned char tmp[20];
	EXPECT_EQ(streamNavigator->getIndex(), progHeaderOffset + headerLen);
	EXPECT_TRUE(streamNavigator->getBytesPdu(tmp, strlen(arr)));
	EXPECT_EQ(strncmp(reinterpret_cast<char *> (tmp), arr, strlen(arr)), 0);
	EXPECT_TRUE(streamNavigator->skipBytesPdu(pduDesc->rpcProgramEndOffset - 
		streamNavigator->getIndex()));
	EXPECT_TRUE(streamNavigator->getBytesPdu(tmp, 1));
	EXPECT_EQ(tmp[0], '@');
	EXPECT_FALSE(streamNavigator->getUint32Pdu(&uint32));
	EXPECT_FALSE(streamNavigator->getBytesPdu(tmp, 1));
	EXPECT_FALSE(streamNavigator->skipBytesPdu(1));

	poolUser->releasePacketDescriptor(pktDesc);
	delete pduDesc;
	delete poolUser;
	delete streamNavigator;
}

TEST(TcpStreamNavigator, multiPacketTest) {
	uint32_t i;
	PcapPacketBufferPool *poolUser;
	PacketDescriptor *pktDesc[4];
	poolUser = PcapPacketBufferPool::registerBufferPool(4);
	for (i = 0; i < 4; i++)
		pktDesc[i] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);

	// constructing a fake multi-packet NFS PDU
	uint32_t pduLen = 34, xid = 0x0a0b0c0d, progVersion = NFS3_VERSION,
		progProc = NFS3PROC_READ;
	uint16_t progHeaderOffset = 4, headerLen = 16;
	uint8_t msgType = RPC_REPLY;
	PduDescriptor *pduDesc = new NfsV3PduDescriptor(pktDesc[0], pktDesc[0],
		pduLen, xid, progVersion, progProc, progHeaderOffset - 1, 
		progHeaderOffset, 0, msgType);
	pktDesc[0]->pcapHeader.caplen = 12;
	pktDesc[0]->payloadOffset = 2;
	pktDesc[1]->pcapHeader.caplen = 12;
	pktDesc[1]->payloadOffset = 2;
	pktDesc[2]->pcapHeader.caplen = 12;
	pktDesc[2]->payloadOffset = 2;
	pktDesc[3]->pcapHeader.caplen = 8;
	pktDesc[3]->payloadOffset = 2;
	pktDesc[0]->next = pktDesc[1];
	pktDesc[1]->next = pktDesc[2];
	pktDesc[2]->next = pktDesc[3];
	pduDesc->lastPktDesc = pktDesc[3];
	pduDesc->rpcProgramEndOffset = pktDesc[3]->pcapHeader.caplen - 1;
	
	// populating the RPC header (4 uint32_t values which span across 2 packets)
	unsigned char *ethFrame = pktDesc[0]->getEthFrameAddress();
	uint32_t uint32;
	i = 0;
	uint32 = htonl(i);	
	memcpy(&ethFrame[progHeaderOffset + i], &uint32, sizeof(uint32));
	i += 4;
	uint32 = htonl(i);	
	memcpy(&ethFrame[progHeaderOffset + i], &uint32, sizeof(uint32));
	i += 4;
	ethFrame = pktDesc[1]->getEthFrameAddress();
	uint32 = htonl(i);
	memcpy(&ethFrame[pktDesc[1]->payloadOffset + 0], &uint32, sizeof(uint32));
	i += 4;
	uint32 = htonl(i);
	memcpy(&ethFrame[pktDesc[1]->payloadOffset + 4], &uint32, sizeof(uint32));
	i += 4;	

	// populating the RPC payload with a uint32_t which spans across 2 packets
	ethFrame[pktDesc[1]->payloadOffset + 8] = 0x0a;
	ethFrame[pktDesc[1]->payloadOffset + 9] = 0x0b;
	ethFrame = pktDesc[2]->getEthFrameAddress();
	ethFrame[pktDesc[2]->payloadOffset] = 0x0c;
	ethFrame[pktDesc[2]->payloadOffset + 1] = 0x0d;
	
	// populating the RPC payload with a byte stream which spans across 2 packets
	memcpy(&ethFrame[pktDesc[2]->payloadOffset + 2], "12345678", 8);
	ethFrame = pktDesc[3]->getEthFrameAddress();
	memcpy(&ethFrame[pktDesc[3]->payloadOffset], "912345", 6);

	// Parsing the RPC header (testing transparent packet cross over)
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	EXPECT_TRUE(streamNavigator->init(pduDesc, progHeaderOffset));
	for (i = 0; i < headerLen; i += 4) { 
		EXPECT_TRUE(streamNavigator->getUint32Pdu(&uint32));
		EXPECT_EQ(uint32, i);
	}
	
	// Parsing RPC payload (testing cross-packet uint32_t)
	EXPECT_TRUE(streamNavigator->getUint32Pdu(&uint32));
	EXPECT_TRUE(uint32 == 0x0a0b0c0d);
	
	// Parsing RPC payload (testing cross-packet byte stream)
	unsigned char tmp[pduLen];
	EXPECT_EQ(streamNavigator->getIndex(), pktDesc[2]->payloadOffset + 2);
	EXPECT_TRUE(streamNavigator->getBytesPdu(tmp, 9));
	EXPECT_EQ(strncmp(reinterpret_cast<char *> (tmp), "123456789", 9), 0);
	EXPECT_TRUE(streamNavigator->skipBytesPdu(4));
	EXPECT_TRUE(streamNavigator->getBytesPdu(tmp, 1));
	EXPECT_EQ(tmp[0], '5');
	EXPECT_FALSE(streamNavigator->getBytesPdu(tmp, 1));

	// Testing cross-packet skip
	EXPECT_TRUE(streamNavigator->init(pduDesc, progHeaderOffset));
	EXPECT_FALSE(streamNavigator->skipBytesPdu(pduLen + 1));
	EXPECT_TRUE(streamNavigator->init(pduDesc, progHeaderOffset));
	EXPECT_TRUE(streamNavigator->skipBytesPdu(pduLen));
	EXPECT_FALSE(streamNavigator->skipBytesPdu(1));

	// Testing cross-packet uint64_t read
	EXPECT_TRUE(streamNavigator->init(pduDesc, progHeaderOffset));
	EXPECT_TRUE(streamNavigator->skipBytesPdu(4));
	uint64_t uint64;
	EXPECT_TRUE(streamNavigator->getUint64Pdu(&uint64));
	EXPECT_TRUE(uint64 == ((4UL << 32) | 8));

	for (i = 0; i < 4; i++)
		poolUser->releasePacketDescriptor(pktDesc[i]);
	delete pduDesc;
	delete poolUser;
	delete streamNavigator;
}
