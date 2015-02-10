// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CPUCACHE_H
#define CPUCACHE_H

#include <inttypes.h>
#include <string>

typedef enum CacheType {NULLCACHE=0, DATA, INST, UNIFIED, OTHER} CacheType; 

/**
 * Represents a CPU cache
 */
class Cache {
	public:	    
		Cache(uint32_t id, uint8_t level, uint32_t size, uint16_t lineSize, 
			CacheType type, uint8_t inclusive);
		~Cache() {};
		/// Prints cache info
		void print();
		/// Retreives cache ID
		/// @returns the cache ID
		uint32_t getCacheID();
		/// Retreives cache level
		/// @returns the cache level
		uint8_t getCacheLevel();
		/// Retreives cache type
		/// @returns the cache type
		CacheType getCacheType();
		static std::string const cacheTypes[5]; 

	private:			
		/// Cache ID
		uint32_t id;
		/// Cache level
		uint8_t level;
		/// Cache size in KB
		uint32_t size;
		/// Cache coherenecy line size in B
		uint16_t coherencyLineSize;
		/// Type of cache (e.g., null, data, instruction, unified)
		CacheType type;
		/// Wether the cache is inclusive of the lower-level caches
		uint8_t inclusive;
};

#endif
