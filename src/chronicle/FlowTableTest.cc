// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include <netinet/in.h>
#include "gtest/gtest.h"
#include "FlowTable.h"
#include "FlowDescriptor.h"
#include "NetworkHeaderParser.h"

TEST(FlowTable, flowTableSubnet10_0_0_0_24) {
	FlowTable *table = new FlowTable();
	unsigned bucketNum, insertedAddrs = 0;
	unsigned srcIP, destIP = 0x0a0b0c0d;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool done = false;
	
	/*
	 * inserting as many 10.0.0.0/24 addresses to exhaust all the ports at a 
	 * filer with 4 NICs
	 */
	for (unsigned h=0; h<4; h++) {
		done = false;
		for (unsigned i=0; i<255 && !done; i++) {
			for (unsigned j=0; j<255 && !done; j++) {
				for (unsigned k=0; k<255 && !done; k++) {
					srcIP = 10 << 24 | i << 16 | i << 8 | k;
					ASSERT_TRUE(NULL == table->lookupFlow(srcIP, destIP+h, 
						srcPort, destPort, IPPROTO_TCP, bucketNum)); 
					FlowDescriptorIPv4 *fd = new FlowDescriptorTcp(srcIP, 
						destIP+h, srcPort, destPort, IPPROTO_TCP);
					table->insertFlow(fd, bucketNum);
					insertedAddrs++;
				
					// also adding the flow in the reverse direction
					ASSERT_TRUE(NULL == table->lookupFlow(destIP+h, srcIP, destPort, 
						srcPort, IPPROTO_TCP, bucketNum)); 
					fd = new FlowDescriptorTcp(destIP+h, 
						srcIP, destPort, srcPort, IPPROTO_TCP);
					table->insertFlow(fd, bucketNum);
					insertedAddrs++;

					if (srcPort == 1) 
						done = true;
					srcPort--;
					destPort++;
				}
			}
		}
	}

	/*
	 * looking up all the 10.0.0.0/24 addresses we inserted
	 */
	srcPort = 0xffff, destPort = 1;
	for (unsigned h=0; h<4; h++) {
		done = false;
		for (unsigned i=0; i<255 && !done; i++) {
			for (unsigned j=0; j<255 && !done; j++) {
				for (unsigned k=0; k<255 && !done; k++) {
					srcIP = 10 << 24 | i << 16 | i << 8 | k;
					ASSERT_TRUE(NULL != table->lookupFlow(srcIP, destIP+h, srcPort, 
						destPort, IPPROTO_TCP, bucketNum)); 
					// also looking up the flow in the reverse direction
					ASSERT_TRUE(NULL != table->lookupFlow(destIP+h, srcIP, destPort, 
							srcPort, IPPROTO_TCP, bucketNum)); 
					if (srcPort == 1) 
						done = true;
					srcPort--;
					destPort++;
				}
			}
		}
	}
	
	FlowTableStat stat;
	table->getFlowTableStats(&stat);
	ASSERT_TRUE(stat.totalNumFlows == insertedAddrs);
	/*
	std::cout.precision(2);
	std::cout << std::fixed
		<< "[10.0.0.0/24 subnet] num flows: " << stat.totalNumFlows 
		<< ", optimal flows per bucket: " << stat.totalNumFlows / table->getSize()
		<< ", max flows per bucket: " << stat.maxNumFlowsInBucket
		<< ", std dev flows per bucket: " << stat.stdDevNumFlowsInBucket
		<< ", empty buckets: " << stat.emptyBuckets << "/"
		<< table->getSize() << std::endl;
	*/
	FlowDescriptorIPv4 *fd, *tmpFd;
	fd = table->getAllFlows();
	while (fd != NULL) { 
		tmpFd = fd;
		fd = fd->next;
		delete tmpFd;
	}
	delete table;
}

TEST(FlowTable, flowTableSubnet192_168_0_0_16) {
	FlowTable *table = new FlowTable();
	unsigned bucketNum, insertedAddrs = 0;
	unsigned srcIP, destIP = 0xc0a80101;	
	uint16_t srcPort = 0xffff, destPort = 1;
	bool done = false;
	
	/*
	 * inserting as many 192.168.0.0/16 addresses to exhaust all the ports at a 
	 * filer with 4 NICs
	 */
	for (unsigned h=0; h<4; h++) {
		done = false;
		for (unsigned i=0; i<255 && !done; i++) {
			for (unsigned j=0; j<255 && !done; j++) {
				srcIP = 192 << 24 | 168 << 16 | i << 8 | j;
				ASSERT_TRUE(NULL == table->lookupFlow(srcIP, destIP+h, srcPort, 
						destPort, IPPROTO_TCP, bucketNum));
				FlowDescriptorIPv4 *fd = new FlowDescriptorTcp(srcIP, 
					destIP+h, srcPort, destPort, IPPROTO_TCP);
				table->insertFlow(fd, bucketNum);
				insertedAddrs++;
				
				// also adding the flow in the reverse direction
				ASSERT_TRUE(NULL == table->lookupFlow(destIP+h, srcIP, 
					destPort, srcPort, IPPROTO_TCP, bucketNum));
				fd = new FlowDescriptorTcp(srcIP, 
					destIP+h, srcPort, destPort, IPPROTO_TCP);
				table->insertFlow(fd, bucketNum);
				insertedAddrs++;
		
				if (srcPort == 1) 
					done = true;
				srcPort--;
				destPort++;
			}
		}
	}
	FlowTableStat stat;
	table->getFlowTableStats(&stat);
	ASSERT_TRUE(stat.totalNumFlows == insertedAddrs);
	/*
	std::cout.precision(2);
	std::cout << std::fixed
		<< "[192.168.0.0/16 subnet] num flows: " << stat.totalNumFlows 
		<< ", optimal flows per bucket: " << stat.totalNumFlows / table->getSize()
		<< ", max flows per bucket: " << stat.maxNumFlowsInBucket
		<< ", std dev flows per bucket: " << stat.stdDevNumFlowsInBucket
		<< ", empty buckets: " << stat.emptyBuckets << "/"
		<< table->getSize() << std::endl;
	*/
	FlowDescriptorIPv4 *fd, *tmpFd;
	fd = table->getAllFlows();
	while (fd != NULL) { 
		tmpFd = fd;
		fd = fd->next;
		delete tmpFd;
	}
	delete table;
}

