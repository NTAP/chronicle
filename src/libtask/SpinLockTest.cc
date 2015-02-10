// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "Lock.h"

TEST(SpinLock, startsUnlocked) {
    TasLock l;
    ASSERT_FALSE(l.isLocked());
}

TEST(SpinLock, canBeLocked) {
    TasLock l;
    l.lock();
    ASSERT_TRUE(l.isLocked());
    l.unlock();
}

TEST(SpinLock, canTryLockUncontended) {
    TasLock l;
    ASSERT_TRUE(l.tryLock());
    l.unlock();
}

TEST(SpinLock, cantTryLockIfLocked) {
    TasLock l;
    l.lock();
    ASSERT_FALSE(l.tryLock());
    l.unlock();
}
