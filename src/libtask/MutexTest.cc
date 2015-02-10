// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "Lock.h"
#include <cerrno>

TEST(Mutex, startsUnlocked) {
    PthreadMutex m;
    ASSERT_TRUE(m.tryLock());
    m.unlock();
}

TEST(Mutex, canBeLockedOnce) {
    PthreadMutex m;
    ASSERT_EQ(0, m.lock());
    ASSERT_FALSE(m.tryLock());
    m.unlock();
}

TEST(Mutex, canBeUnLocked) {
    PthreadMutex m;
    m.lock();
    m.unlock();
    ASSERT_TRUE(m.tryLock());
    m.unlock();
}

TEST(Mutex, canBeLockedByTryLock) {
    PthreadMutex m;
    ASSERT_TRUE(m.tryLock());
    ASSERT_FALSE(m.tryLock());
    m.unlock();
}

TEST(Mutex, mustBeUnlockedBeforeDestruction) {
    PthreadMutex *m = new PthreadMutex;
    m->lock();
    ASSERT_DEATH({delete m;}, "Perhaps the Mutex is locked\\?");
    m->unlock();
    delete m;
}
