// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "Cache.h"

TEST(Cache, initCache) {
    Cache *c = new Cache(0xa, 1, 32*1024, 64, INST, 1);
    ASSERT_EQ(0xa, static_cast <int> (c->getCacheID()));
}


