// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <list>
#include <unistd.h>
#include "gtest/gtest.h"
#include "TopologyMap.h"

TEST(TopologyMap, initTopologyMap) {
	TopologyMap *map = new TopologyMap();
	ASSERT_EQ(sysconf(_SC_NPROCESSORS_CONF), map->init()); 
}

TEST(TopologyMap, sameCore) {
	std::list<int> l;
	TopologyMap *map = new TopologyMap();

	ASSERT_EQ(sysconf(_SC_NPROCESSORS_CONF), map->init()); 
	ASSERT_EQ(0, map->sameCore(0, l));		//TODO: more extensive google mock tests
}
