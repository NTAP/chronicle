// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FLOW_TABLE_H
#define FLOW_TABLE_H

#include <inttypes.h>
#include "ChronicleConfig.h"

class FlowDescriptorIPv4;

class FlowTableBucket {
	public:
		FlowTableBucket() 
			{ flowDescriptor = NULL; numFlows = 0; }
		FlowDescriptorIPv4 *flowDescriptor;
		uint16_t numFlows;
};

class FlowTableStat {
	public: 
		FlowTableStat() { }
		uint32_t totalNumFlows;
		double stdDevNumFlowsInBucket;
		uint32_t maxNumFlowsInBucket;
		uint32_t emptyBuckets;
};

class FlowTable {
	public:
		FlowTable(unsigned size = FLOW_TABLE_DEFAULT_SIZE);
		~FlowTable();
		FlowDescriptorIPv4 *lookupFlow(uint32_t srcIP, uint32_t destIP, 
			uint16_t srcPort, uint16_t destPort, uint8_t protocol, 
			unsigned &bucketNum);
		void insertFlow(FlowDescriptorIPv4 *flowDesc, unsigned bucketNum);
		/// chains all flow descriptors together (used during cleanup)
		FlowDescriptorIPv4 *getAllFlows();
		void getFlowTableStats(FlowTableStat *stat);
		unsigned getSize() { return size; }
		FlowDescriptorIPv4 *getBucketHead(unsigned bucketNum) 
			{ return flowTable[bucketNum].flowDescriptor; }
		void removeFlowDesc(unsigned bucketNum, FlowDescriptorIPv4 *flowDesc);

	private:
		unsigned size;
		unsigned numFlows;
		FlowTableBucket *flowTable;	
};

#endif // FLOW_TABLE_H
