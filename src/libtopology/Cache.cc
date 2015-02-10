// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include "Cache.h"

std::string const Cache::cacheTypes[5] =
	{"NULL", "DATA", "INST", "UNIFIED", "OTHER"};

Cache::Cache(uint32_t id, uint8_t level, uint32_t size, uint16_t lineSize, 
	CacheType type, uint8_t inclusive) :
	id(id), level(level), size(size), coherencyLineSize(lineSize), 
	type(type), inclusive(inclusive)
{}

void
Cache::print()
{
	printf("\t[L%u]: id=0x%x, type=%s, size=%uKB, line=%uB, inclusive=%u\n",
		level, id, cacheTypes[type].c_str(), size/1024, coherencyLineSize, inclusive);
}

uint32_t
Cache::getCacheID()
{
	return id;
}

uint8_t
Cache::getCacheLevel()
{
	return level;
}

CacheType
Cache::getCacheType()
{
	return type;
}

