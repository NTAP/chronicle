// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include "gtest/gtest.h"
#include "ChronicleProcessRequest.h"
#include "PcapPacketBufferPool.h"

TEST(PcapPacketBufferPool, onePoolUserSetupTeardown) {
	PcapPacketBufferPool *pool;
	pool = PcapPacketBufferPool::registerBufferPool(2);
    ASSERT_TRUE(pool != NULL);
	ASSERT_TRUE(pool->unregisterBufferPool());
	delete pool;
}

TEST(PcapPacketBufferPool, twoPoolUsersSetupTeardown) {
	PcapPacketBufferPool *poolUser1, *poolUser2;
	poolUser1 = PcapPacketBufferPool::registerBufferPool(2);
	poolUser2 = PcapPacketBufferPool::registerBufferPool(2);
    ASSERT_TRUE(poolUser1 != NULL);
	ASSERT_TRUE(poolUser2 == poolUser1);
	ASSERT_FALSE(poolUser1->unregisterBufferPool());
	ASSERT_TRUE(poolUser2->unregisterBufferPool());
	delete poolUser2;
}

TEST(PcapPacketBufferPool, pktDescriptorsCheck) {
	// buf pool setup
	PcapPacketBufferPool *poolUser1, *poolUser2;
	poolUser1 = PcapPacketBufferPool::registerBufferPool(4);
	poolUser2 = PcapPacketBufferPool::registerBufferPool(4);
    ASSERT_TRUE(poolUser1 != NULL);
	ASSERT_TRUE(poolUser2 == poolUser1);

	// descriptor allocation
	PacketDescriptor *descriptors[4];
	descriptors[0] = poolUser1->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[0]->packetBufferIndex == 0);
	descriptors[1] = poolUser1->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[1]->packetBufferIndex == 1);
	descriptors[2] = poolUser2->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[2]->packetBufferIndex == 2);
	descriptors[3] = poolUser2->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[3]->packetBufferIndex == 3);
	ASSERT_TRUE(poolUser2->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND) == NULL);

	// descriptor deallocation in reverse order
	poolUser2->releasePacketDescriptor(descriptors[3]);
	poolUser2->releasePacketDescriptor(descriptors[2]);
	poolUser1->releasePacketDescriptor(descriptors[1]);
	poolUser1->releasePacketDescriptor(descriptors[0]);

	// descriptor reallocation
	EXPECT_TRUE(poolUser1->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND)->packetBufferIndex == 3);
	EXPECT_TRUE(poolUser1->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND)->packetBufferIndex == 2);
	EXPECT_TRUE(poolUser2->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND)->packetBufferIndex == 1);
	EXPECT_TRUE(poolUser2->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND)->packetBufferIndex == 0);

	// releasing all the descriptors
	for(int i=0; i<4; i++)
		poolUser1->releasePacketDescriptor(descriptors[i]);

	ASSERT_FALSE(poolUser1->unregisterBufferPool());
	ASSERT_TRUE(poolUser2->unregisterBufferPool());
	delete poolUser2;	
}

TEST(PcapPacketBufferPool, pktDataCheck) {
	// buf pool setup
	PcapPacketBufferPool *poolUser;
	poolUser = PcapPacketBufferPool::registerBufferPool(4);

	// descriptor allocation
	PacketDescriptor *descriptors[4];
	descriptors[0] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[0]->packetBufferIndex == 0);
	descriptors[1] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[1]->packetBufferIndex == 1);
	descriptors[2] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[2]->packetBufferIndex == 2);
	descriptors[3] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
	EXPECT_TRUE(descriptors[3]->packetBufferIndex == 3);
	ASSERT_TRUE(poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND) == NULL);

	// writing data to packet buffers
	for (int i = 0; i < 4; i++) {
		char *addr, str[10];
		addr = reinterpret_cast<char *> (descriptors[i]->getEthFrameAddress());
		sprintf(str, "%s %d", "packet", i);
		strcpy(addr, str);
	}

	// descriptor deallocation in order
	poolUser->releasePacketDescriptor(descriptors[0]);
	poolUser->releasePacketDescriptor(descriptors[1]);
	poolUser->releasePacketDescriptor(descriptors[2]);
	poolUser->releasePacketDescriptor(descriptors[3]);

	// descriptor reallocation and validating packet content
	for (int i = 0; i < 4; i++) {
		char *addr, str[10];
		descriptors[i] = poolUser->getPacketDescriptor(MAX_ETH_FRAME_SIZE_STAND);
		addr = reinterpret_cast<char *> (descriptors[i]->getEthFrameAddress());
		sprintf(str, "%s %d", "packet", i);
		EXPECT_TRUE(strcmp(addr, str) == 0);
	}

	// releasing all the descriptors
	for(int i=0; i<4; i++)
		poolUser->releasePacketDescriptor(descriptors[i]);

	ASSERT_TRUE(poolUser->unregisterBufferPool());
	delete poolUser;	
}

/* 
 * commented out as root privilege is required for restoring original rlimits
 * so that the tests can be run in any random order.
 */
/*
TEST(PcapPacketBufferPool, tooManyBufferDescriptors) {
	struct rlimit addressSpaceLim = {10000, 10000}, origAddressSpaceLim;
	getrlimit(RLIMIT_AS, &origAddressSpaceLim);
	setrlimit(RLIMIT_AS, &addressSpaceLim);
	PcapPacketBufferPool *pool;
	pool = PcapPacketBufferPool::registerBufferPool(20000);
	ASSERT_TRUE(pool == NULL);
	if (setrlimit(RLIMIT_AS, &origAddressSpaceLim) == -1) // requires privileged process
		perror("setrlimit");
}

TEST(PcapPacketBufferPool, tooManyPackets) {
	struct rlimit addressSpaceLim = {10000, 10000}, origAddressSpaceLim;
	getrlimit(RLIMIT_AS, &origAddressSpaceLim);
	setrlimit(RLIMIT_AS, &addressSpaceLim);
	PcapPacketBufferPool *pool;
	pool = PcapPacketBufferPool::registerBufferPool(100);
	ASSERT_TRUE(pool == NULL);
	if (setrlimit(RLIMIT_AS, &origAddressSpaceLim) == -1) // requires privileged process
		perror("setrlimit");
}
*/
