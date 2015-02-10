// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "Process.h"
#include "Message.h"
#include "Scheduler.h"

bool ptInited = Scheduler::initScheduler();

TEST(Process, newProcessIsNotRunnable) {
    Process p;
    ASSERT_FALSE(p.isRunnable());
}

class NullMessage : public Message {
public:
    void run() { }
};

TEST(Process, processWithPendingMsgIsRunnable) {
    Process p;
    Message *m = new NullMessage;
    p.enqueueMessage(m, NULL);
    ASSERT_TRUE(p.isRunnable());
    delete m;
}

class CountingMsg : public Message {
public:
    void run() { ++counter; }
    static unsigned counter;
};
unsigned CountingMsg::counter = 0;

TEST(Process, processCanExecuteMessage) {
    CountingMsg::counter = 0;
    Process p;
    Message *m = new CountingMsg;
    p.enqueueMessage(m, NULL);
    ASSERT_TRUE(p.runOneMessage());
    ASSERT_EQ(1u, CountingMsg::counter);
    ASSERT_FALSE(p.runOneMessage());
    ASSERT_EQ(1u, CountingMsg::counter);
}

TEST(Process, countsNumThatAreAlive) {
    ASSERT_EQ(0u, Process::numProcesses());
    Process *p = new Process;
    Process *p2 = new Process;
    ASSERT_EQ(2u, Process::numProcesses());
    delete p;
    delete p2;
    ASSERT_EQ(0u, Process::numProcesses());
}

TEST(Process, newProcessHasNotExited) {
    Process p;
    ASSERT_FALSE(p.hasExited());
}

TEST(Process, processCanExit) {
    Process p;
    p.exit();
    ASSERT_TRUE(p.hasExited());
}
