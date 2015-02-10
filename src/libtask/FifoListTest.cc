// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "FifoList.h"

class TestNode : public FifoNode<TestNode> {
public:
    unsigned val;
};

TEST(FifoList, newListIsEmpty) {
    FifoList<TestNode> l;
    ASSERT_TRUE(l.isEmpty());
    ASSERT_EQ(NULL, l.consume());
}

TEST(FifoList, canInsertOneNode) {
    FifoList<TestNode> l;
    TestNode n, *np;
    n.val = 2;
    l.insert(&n);
    np = l.consume();
    ASSERT_EQ(&n, np);
    ASSERT_EQ(2u, np->val);
    ASSERT_EQ(NULL, l.consume());
}

TEST(FifoList, isAFifo) {
    FifoList<TestNode> l;
    TestNode a, b, c;
    a.val = 4;
    b.val = 5;
    c.val = 6;
    l.insert(&a);
    l.insert(&b);
    l.insert(&c);
    ASSERT_EQ(&a, l.consume());
    ASSERT_EQ(&b, l.consume());
    ASSERT_EQ(&c, l.consume());
    ASSERT_EQ(NULL, l.consume());
    ASSERT_EQ(5u, b.val);
}

TEST(FifoList, preventsMultipleInsertion) {
    FifoList<TestNode> l;
    TestNode a, b, c;
    a.val = 4;
    b.val = 5;
    c.val = 6;
    l.insert(&a);
    l.insert(&b);
    l.insert(&c);
    ASSERT_DEATH({l.insert(&a);}, "0 == node->_currList");
}
