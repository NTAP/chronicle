// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef LOGICALPROCESSOR_H
#define LOGICALPROCESSOR_H

#include <inttypes.h>
#include <list>
#include "Cache.h"

/**
 * Represents a logical processor 
 * A LogicalProcessor can be an SMT thread on a HT core, a regurlar core in 
 * absence of HT, or a physical processor in absence of HT and multicore.
 */
class LogicalProcessor {
	public:
		LogicalProcessor(uint8_t level, int logicalProcessorNum, uint8_t threadID, 
			uint8_t coreID, uint8_t socketID);
		~LogicalProcessor();
		/** Inserts a cache into a logical processor
		 * @param[in] id Cache ID
		 * @param[in] level Cache level
		 * @param[in] size Cache size
		 * @param[in] lineSize Cache line size
		 * @param[in] type Cacye type (i.e., instruction, data, unified, other)
		 * @param[in] inclusive the cache inclusive of the lower-level caches (0 or 1)
		 */
		void insertCache(uint32_t id, uint8_t level, uint32_t size, uint16_t lineSize, 
			CacheType type, uint8_t inclusive);
		/** Prints LogicalProcessor information */
		void print();
		/** 
		 * Inserts a cache entry into the corresponding LogicalProcessor
		 * @param[in] c A cache for this LogicalProcessor
		 */
		void insertCache(Cache *c);
		/** 
		 * Given a cache level and cache type returns the Cache object
		 * @param[in] level The cache level
		 * @param[in] type The cache type
		 * @returns the matching Cache object
		 */
		Cache* getCache(uint8_t level, CacheType type);
		void printCache();
		uint8_t getLogicalProcessorID();
		uint8_t getThreadID();
		uint8_t getCoreID();
		uint8_t getSocketID();

	private:
		/// the depth of hierarchy in CPU topology (e.g., level=3 when we have thread, core, socket)
		uint8_t processorLevels;
		/// logical processor number as viewed by Linux
		int logicalProcessorID; 
		/// APIC ID for thread
		uint8_t threadID;
		/// APIC ID for core
		uint8_t coreID;
		/// APIC ID for socket
		uint8_t socketID;
		/// the number of cache levels
		uint8_t cacheLevels;
		/// a list of caches corresponding to a LogicalProcessor object
		std::list<Cache *> caches;
};
#endif
