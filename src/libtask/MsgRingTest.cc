// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "MsgRingApp.h"
#include "Scheduler.h"

bool mrtInited = Scheduler::initScheduler();

class MockRingMember : public MsgRingMember {
public:
    MOCK_METHOD3(createLink, void(unsigned remainingNodes,
                                  MsgRingMemberFactory *factory,
                                  MsgRingApp *master));
    MOCK_METHOD1(passToken, void(unsigned passes));
    MOCK_METHOD0(shutdown, void());
};

class RMInjector : public MsgRingMemberFactory {
public:
    RMInjector(MsgRingMember *rm) : _rm(rm) { }
    virtual MsgRingMember *operator()() {
        return _rm;
    }
private:
    MsgRingMember *_rm;
};


TEST(MsgRingApp, canInstantiateApp) {
    MsgRingApp *mrApp = new MsgRingApp();
    ASSERT_NE(static_cast<void *>(NULL), mrApp);
    delete mrApp;
}


class MsgRingAppF : public testing::Test {
protected:
    virtual void SetUp() {
        Scheduler::setCurrentScheduler(NULL);
        mra = new MsgRingApp();
        mrm = new MockRingMember();
        rmi = new RMInjector(mrm);
    }
    virtual void TearDown() {
        delete rmi;
        delete mrm;
        delete mra;
    }
    MsgRingApp *mra;
    RMInjector *rmi;
    MockRingMember *mrm;
};

TEST_F(MsgRingAppF, createWithZeroLinksFails) {
    //::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH({mra->createLink(0, rmi, mra);}, "Too few nodes in ring");
    ASSERT_FALSE(mra->isRunnable());
}

TEST_F(MsgRingAppF, createsLinks) {
    EXPECT_CALL(*mrm, createLink(0, rmi, mra)).Times(1);
    mra->createLink(1, rmi, mra);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_FALSE(mra->isRunnable());
}

TEST_F(MsgRingAppF, passesToken) {
    // Have to create a link for App to use
    EXPECT_CALL(*mrm, createLink(0, rmi, mra)).Times(1);
    mra->run(2, 10, 1, NULL, rmi);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_TRUE(mra->runOneMessage());
    // Token should always have value of 1
    EXPECT_CALL(*mrm, passToken(1)).Times(1);
    mra->passToken(3);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_FALSE(mra->isRunnable());
}

class CbChecker : public MsgRingApp::MsgRingResult {
public:
    bool _callbackCalled;
    CbChecker() : _callbackCalled(false) { }
    void operator()() { _callbackCalled = true; }
};
TEST_F(MsgRingAppF, callsCbAndShutdown) {
    CbChecker cb;
    // Have to create a link for App to use
    EXPECT_CALL(*mrm, createLink(0, rmi, mra)).Times(1);
    mra->run(2, 10, 1, &cb, rmi);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_TRUE(mra->runOneMessage());
    // Token should always have value of 1
    EXPECT_CALL(*mrm, passToken(1)).Times(4);
    mra->ringReady();
    ASSERT_TRUE(mra->runOneMessage());
    mra->passToken(3);
    ASSERT_TRUE(mra->runOneMessage());
    mra->passToken(3);
    ASSERT_TRUE(mra->runOneMessage());
    mra->passToken(3);
    ASSERT_TRUE(mra->runOneMessage());
    mra->passToken(5);
    EXPECT_CALL(*mrm, shutdown()).Times(1);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_FALSE(mra->runOneMessage());
    ASSERT_TRUE(cb._callbackCalled);
}

TEST_F(MsgRingAppF, shutdown) {
    // Have to create a link for App to use
    EXPECT_CALL(*mrm, createLink(0, rmi, mra)).Times(1);
    mra->createLink(1, rmi, mra);
    ASSERT_TRUE(mra->runOneMessage());
    mra->shutdown();
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_FALSE(mra->isRunnable());
    ASSERT_TRUE(mra->hasExited());
}

TEST_F(MsgRingAppF, sendsBatchOfTokens) {
    mra->run(10, 100, 5, NULL, rmi);
    ASSERT_TRUE(mra->runOneMessage());
    EXPECT_CALL(*mrm, createLink(8, rmi, mra)).Times(1);
    ASSERT_TRUE(mra->runOneMessage());
    mra->ringReady();
    EXPECT_CALL(*mrm, passToken(1)).Times(5);
    ASSERT_TRUE(mra->runOneMessage());
    ASSERT_FALSE(mra->runOneMessage());
}

class MockRingApp : public MsgRingApp {
public:
    MOCK_METHOD3(createLink, void(unsigned remainingNodes,
                                  MsgRingMemberFactory *factory,
                                  MsgRingApp *master));
    MOCK_METHOD1(passToken, void(unsigned passes));
    MOCK_METHOD0(shutdown, void());
    MOCK_METHOD5(run, void (unsigned loop_size,
                            unsigned messages,
                            unsigned batch_size,
                            MsgRingResult *cb,
                            MsgRingMemberFactory *factory));
    MOCK_METHOD0(ringReady, void());

};

class MsgRingWorkerF : public testing::Test {
protected:
    virtual void SetUp() {
        Scheduler::setCurrentScheduler(NULL);
    }
};

TEST_F(MsgRingWorkerF, createLinkInternal) {
    MsgRingApp a;
    MsgRingWorker w;
    MockRingMember mrm;
    RMInjector rmi(&mrm);
    EXPECT_CALL(mrm, createLink(2, &rmi, &a)).Times(1);
    w.createLink(3, &rmi, &a);
    ASSERT_TRUE(w.runOneMessage());
    ASSERT_FALSE(w.runOneMessage());
}

TEST_F(MsgRingWorkerF, createLinkLast) {
    MockRingApp a;
    MsgRingWorker w;
    EXPECT_CALL(a, ringReady()).Times(1);
    w.createLink(0, NULL, &a);
    ASSERT_FALSE(w.runOneMessage());
}

TEST_F(MsgRingWorkerF, canPassToken) {
    MsgRingWorker w;
    MockRingMember mrm;
    RMInjector rmi(&mrm);
    EXPECT_CALL(mrm, createLink(2, &rmi, NULL)).Times(1);
    w.createLink(3, &rmi, NULL);
    ASSERT_TRUE(w.runOneMessage());
    w.passToken(7);
    EXPECT_CALL(mrm, passToken(8)).Times(1);
    ASSERT_TRUE(w.runOneMessage());
    ASSERT_FALSE(w.runOneMessage());
}

TEST_F(MsgRingWorkerF, shutdown) {
    MsgRingWorker w;
    MockRingMember mrm;
    RMInjector rmi(&mrm);
    EXPECT_CALL(mrm, createLink(2, &rmi, NULL)).Times(1);
    w.createLink(3, &rmi, NULL);
    ASSERT_TRUE(w.runOneMessage());
    EXPECT_CALL(mrm, shutdown()).Times(1);
    w.shutdown();
    ASSERT_TRUE(w.runOneMessage());
    ASSERT_FALSE(w.runOneMessage());
    ASSERT_TRUE(w.hasExited());
}

TEST_F(MsgRingWorkerF, canGetAFactory) {
    MsgRingMemberFactory *f = MsgRingWorker::getFactory();
    ASSERT_TRUE(NULL != f);
    MsgRingMember *m = (*f)();
    ASSERT_TRUE(NULL != m);
    delete m;
}
