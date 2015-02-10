// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "LogicalProcessor.h"

TEST(LogicalProcessor, initLP) {
    LogicalProcessor *l = new LogicalProcessor(3, 0, 12, 8, 1);
    ASSERT_EQ(12, static_cast <int> (l->getThreadID()));
}

TEST(LogicalProcessor, insertCache) {
    LogicalProcessor *l = new LogicalProcessor(3, 0, 12, 8, 1);
	l->insertCache(0xa, 1, 32*1024, 64, INST, 1);
	l->insertCache(0xa, 1, 32*1024, 64, DATA, 1);
	l->insertCache(0xa, 2, 256*1024, 64, DATA, 1);
	l->insertCache(0xa, 3, 6*1024*1024, 64, UNIFIED, 1);
	
	Cache *c = l->getCache(1, INST);
	ASSERT_TRUE(c != NULL);
	c =  l->getCache(1, UNIFIED);
	ASSERT_FALSE(c != NULL);
}

