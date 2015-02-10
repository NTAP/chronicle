// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include "LogicalProcessor.h"

LogicalProcessor::LogicalProcessor(uint8_t level, int logicalProcessorID, 
	uint8_t threadID=0, uint8_t coreID=0, uint8_t socketID=0) :
	processorLevels(level), logicalProcessorID(logicalProcessorID),
	threadID(threadID),	coreID(coreID), socketID(socketID)
{ }

LogicalProcessor::~LogicalProcessor()
{
	std::list<Cache *>::const_iterator it;

	for (it = caches.begin(); it != caches.end(); it++) 
		delete *it;
}

void
LogicalProcessor::print()
{
	printf("(logical) processor=%u\t[threadID=0x%x coreID=0x%x socketID=0x%x]:\n", logicalProcessorID, threadID, coreID, socketID);
	this->printCache();
}

void
LogicalProcessor::insertCache(uint32_t id, uint8_t level, uint32_t size, 
	uint16_t lineSize, CacheType type, uint8_t inclusive)
{
	Cache *c = new Cache(id, level, size, lineSize, type, inclusive);
	caches.push_back(c);
}

Cache*
LogicalProcessor::getCache(uint8_t level, CacheType type)
{
	std::list<Cache *>::const_iterator it;

	for (it = caches.begin(); it != caches.end(); it++)
		if ((*it)->getCacheLevel() == level && (*it)->getCacheType() == type)
			return *it;

	return NULL;	
}

void
LogicalProcessor::printCache()
{
	std::list<Cache *>::iterator it;

	for (it = caches.begin(); it != caches.end(); it++)
		(*it)->print();
}

uint8_t
LogicalProcessor::getLogicalProcessorID()
{
	return logicalProcessorID;
}

uint8_t
LogicalProcessor::getThreadID()
{
	return threadID;
}

uint8_t
LogicalProcessor::getCoreID()
{
	return coreID;
}

uint8_t
LogicalProcessor::getSocketID()
{
	return socketID;
}
