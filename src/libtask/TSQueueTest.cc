// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "TSQueue.h"

TEST(TSQueue, newQueueIsEmpty) {
    TSQueue<unsigned> q;
    ASSERT_EQ(true, q.isEmpty());
}

TEST(TSQueue, queueWith1ElementIsNotEmpty) {
    TSQueue<unsigned> q;
    unsigned v = 5;
    q.enqueue(&v);
    ASSERT_FALSE(q.isEmpty());
}

TEST(TSQueue, canEnqueAndDequeue1Element) {
    TSQueue<unsigned> q;
    unsigned v = 5;
    q.enqueue(&v);
    unsigned *val = q.dequeue();
    ASSERT_EQ(5u, *val);
}

TEST(TSQueue, isOrderedFIFO) {
    TSQueue<unsigned> q;
    unsigned v1 = 1, v2 = 2, v3 = 3, v4 = 4;
    q.enqueue(&v1);
    q.enqueue(&v3);
    q.enqueue(&v4);
    q.enqueue(&v2);
    ASSERT_EQ(1u, *q.dequeue());
    ASSERT_EQ(3u, *q.dequeue());
    ASSERT_EQ(4u, *q.dequeue());
    ASSERT_EQ(2u, *q.dequeue());
}

TEST(TSQueue, isEmptyAfterDequeue) {
    TSQueue<unsigned> q;
    unsigned v1 = 1, v3 = 3;
    q.enqueue(&v1);
    q.enqueue(&v3);
    ASSERT_FALSE(q.isEmpty());
    q.dequeue();
    q.dequeue();
    ASSERT_TRUE(q.isEmpty());
}

TEST(TSQueue, dequeueEmptyIsNull) {
    TSQueue<unsigned> q;
    ASSERT_EQ(NULL, q.dequeue());
}
