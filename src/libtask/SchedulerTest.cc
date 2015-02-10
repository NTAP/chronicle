// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "Scheduler.h"
#include "Process.h"
#include "Message.h"

bool sInited = Scheduler::initScheduler();

TEST(Scheduler, newSchedulerHasEmptyRunQueue) {
    Scheduler s;
    ASSERT_EQ(NULL, s.getNextProcess());
}

TEST(Scheduler, canGetNextProcess) {
    Scheduler s;
    Process p1;
    s.enqueueProcess(&p1);
    ASSERT_EQ(&p1, s.getNextProcess());
    ASSERT_EQ(NULL, s.getNextProcess());
}

TEST(Scheduler, runQueueIsFifo) {
    Scheduler s;
    Process p1, p2, p3;
    s.enqueueProcess(&p1);
    s.enqueueProcess(&p2);
    s.enqueueProcess(&p3);
    ASSERT_EQ(&p1, s.getNextProcess());
    ASSERT_EQ(&p2, s.getNextProcess());
    ASSERT_EQ(&p3, s.getNextProcess());
}

class NullMessage : public Message {
public:
    void run() { }
};
class ExitMsg : public Message {
public:
    ExitMsg(Process *p) : _p(p) { }
    Process *_p;
    void run() { _p->exit(); }
};
TEST(Scheduler, runReturnsAfterLastProcess) {
    Scheduler s;
    Process *p = new Process;
    p->enqueueMessage(new NullMessage);
    p->enqueueMessage(new ExitMsg(p));
    s.enqueueProcess(p);
    s.run();
}
