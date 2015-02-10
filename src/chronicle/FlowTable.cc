// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstddef>
#include <iostream>
#include <cmath>
#include "FlowTable.h"
#include "FlowDescriptor.h"
#include "BobJenkinsHashLookup3.h"

uint16_t hash16BitBobJenkins(uint32_t srcIP, uint32_t destIP, uint16_t srcPort, 
	uint16_t destPort, uint8_t protocol);

FlowTable::FlowTable(unsigned size) : size(size)
{
	numFlows = 0;
	flowTable = new FlowTableBucket[size];
}

FlowTable::~FlowTable()
{
	
	delete[] flowTable;
}

FlowDescriptorIPv4 *
FlowTable::lookupFlow(uint32_t srcIP, uint32_t destIP, uint16_t srcPort, 
	uint16_t destPort, uint8_t protocol, unsigned &bucketNum)
{
	bucketNum = hash16BitBobJenkins(srcIP, destIP, srcPort, destPort, protocol);

	// find the flow descriptor
	FlowDescriptorIPv4 *flowDescriptor = flowTable[bucketNum].flowDescriptor;
	while (flowDescriptor != NULL) {
		if (flowDescriptor->match(srcIP, destIP, srcPort, destPort, protocol))
			break;
		else
			flowDescriptor = flowDescriptor->next;
	}
	return flowDescriptor;	
}

void 
FlowTable::insertFlow(FlowDescriptorIPv4 *flowDescriptor, unsigned bucketNum)
{
	flowDescriptor->next = flowTable[bucketNum].flowDescriptor;
	flowTable[bucketNum].flowDescriptor = flowDescriptor;
	flowTable[bucketNum].numFlows++;
	numFlows++;
}

FlowDescriptorIPv4 *
FlowTable::getAllFlows()
{
	FlowDescriptorIPv4 *flowDescriptor, *firstFlowDescriptor = NULL, 
		*lastFlowDescriptor = NULL;
	for (unsigned i = 0; i < size; i++) {
		flowDescriptor = flowTable[i].flowDescriptor;
		if (firstFlowDescriptor == NULL && flowDescriptor != NULL) {
			firstFlowDescriptor = flowDescriptor;
		} else if (flowDescriptor != NULL) {
			lastFlowDescriptor->next = flowDescriptor;
		}
		while (flowDescriptor != NULL) {
			lastFlowDescriptor = flowDescriptor;
			flowDescriptor = flowDescriptor->next;
		} 
	}
	return firstFlowDescriptor;
}

void
FlowTable::getFlowTableStats(FlowTableStat *stat)
{
	uint32_t max = 0, avg, emptyBuckets = 0;
	double stddev = 0;
	avg = numFlows / size;
	for (unsigned i = 0; i < size; i++) {
		if (flowTable[i].numFlows > max)
			max = flowTable[i].numFlows;
		if (flowTable[i].numFlows == 0)
			emptyBuckets++;
		stddev += (flowTable[i].numFlows - avg) * (flowTable[i].numFlows - avg);
	}
	
	stat->totalNumFlows = numFlows;
	stat->stdDevNumFlowsInBucket = sqrt(stddev / numFlows);
	stat->maxNumFlowsInBucket = max;
	stat->emptyBuckets = emptyBuckets;
}

void
FlowTable::removeFlowDesc(unsigned bucketNum, FlowDescriptorIPv4 *flowDesc)
{
	FlowDescriptorIPv4 *tmpFlowDesc = flowTable[bucketNum].flowDescriptor;
	if (tmpFlowDesc == NULL)
		return ;
	else if (tmpFlowDesc == flowDesc) {
		flowTable[bucketNum].flowDescriptor = flowDesc->next;
		delete tmpFlowDesc;
		return ;
	}
	while (tmpFlowDesc->next != flowDesc && tmpFlowDesc != NULL)
		tmpFlowDesc = tmpFlowDesc->next;
	if (tmpFlowDesc == NULL)
		return ;
	tmpFlowDesc->next = flowDesc->next;
	delete flowDesc;
}

uint16_t
hash16BitBobJenkins(uint32_t srcIP, uint32_t destIP, uint16_t srcPort, 
	uint16_t destPort, uint8_t protocol)
{
	mix(srcIP, destIP, srcPort);
	srcIP += destPort; destIP += protocol;
	final(srcIP, destIP, srcPort);
	return srcPort;	
}
