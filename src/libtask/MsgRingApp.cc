// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "MsgRingApp.h"
#include <cassert>
#include <sys/time.h>
#include <iostream>

class MsgRingApp::MessageBase : public Message {
protected:
    MsgRingApp *_who;
public:
    MessageBase(MsgRingApp *who) : _who(who) { }
};



class MsgRingApp::MessageCreateLink : public MessageBase {
protected:
    unsigned _remainingNodes;
    MsgRingMemberFactory *_factory;
    MsgRingApp *_master;
public:
    MessageCreateLink(MsgRingApp *who,
                      unsigned remainingNodes,
                      MsgRingMemberFactory *factory,
                      MsgRingApp *master) :
        MessageBase(who), _remainingNodes(remainingNodes), _factory(factory),
        _master(master) { }
    virtual void run() {
        _who->doCreateLink(_remainingNodes, _factory, _master);
    }
};
void
MsgRingApp::createLink(unsigned remainingNodes,
                       MsgRingMemberFactory *factory,
                       MsgRingApp *master)
{
    assert(remainingNodes && "Too few nodes in ring.");
    enqueueMessage(new MessageCreateLink(this, remainingNodes, factory,
                                         master));
}
void
MsgRingApp::doCreateLink(unsigned remainingNodes,
                         MsgRingMemberFactory *factory,
                         MsgRingApp *master)
{
    _next = (*factory)();
    _next->createLink(remainingNodes-1, factory, master);
}



class MsgRingApp::MessagePassToken : public MessageBase {
protected:
    unsigned _passes;
public:
    MessagePassToken(MsgRingApp *who, unsigned passes) :
        MessageBase(who), _passes(passes) { }
    virtual void run() { _who->doPassToken(_passes); }
};
void
MsgRingApp::passToken(unsigned passes)
{
    enqueueMessage(new MessagePassToken(this, passes));
}
void
MsgRingApp::doPassToken(unsigned passes)
{
    _totalMessages += passes;
    if (_totalMessages < _targetMessages) {
        _next->passToken(1);
    } else {
        _outstandingMessages--;
        if (0 == _outstandingMessages) {
            if (_cb) {
                _cb->_elapsedTime = timeNow() - _startTime;
                _cb->_totalMessages = _totalMessages;
                (*_cb)();
            }
            _next->shutdown();
        }
    }
}

class MsgRingApp::MessageRun : public MessageBase {
protected:
    unsigned _loopSize;
    unsigned _messages;
    unsigned _batchSize;
    MsgRingResult *_cb;
    MsgRingMemberFactory *_factory;
public:
    MessageRun(MsgRingApp *who,
               unsigned loop_size,
               unsigned messages,
               unsigned batch_size,
               MsgRingResult *cb,
               MsgRingMemberFactory *factory) :
        MessageBase(who), _loopSize(loop_size), _messages(messages),
        _batchSize(batch_size), _cb(cb), _factory(factory) { }
    virtual void run() {
        _who->doRun(_loopSize, _messages, _batchSize, _cb, _factory);
    }
};
void
MsgRingApp::run(unsigned loop_size,
                unsigned messages,
                unsigned batch_size,
                MsgRingResult *cb,
                MsgRingMemberFactory *factory)
{
    enqueueMessage(new MessageRun(this, loop_size, messages, batch_size, cb,
                                  factory));
}
void
MsgRingApp::doRun(unsigned loop_size,
                  unsigned messages,
                  unsigned batch_size,
                  MsgRingResult *cb,
                  MsgRingMemberFactory *factory)
{
    _loopSize = loop_size;
    _targetMessages = messages;
    _batchSize = batch_size;
    _cb = cb;
    _outstandingMessages = 0;
    _totalMessages = 0;

    // Build the ring
    createLink(loop_size-1, factory, this);
}



class MsgRingApp::MessageShutdown : public MessageBase {
public:
    MessageShutdown(MsgRingApp *who) : MessageBase(who) { }
    virtual void run() { _who->doShutdown(); }
};
void
MsgRingApp::shutdown()
{
    enqueueMessage(new MessageShutdown(this));
}
void
MsgRingApp::doShutdown()
{
    exit();
}


double
MsgRingApp::timeNow()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return static_cast<double>(tv.tv_sec) +
        static_cast<double>(tv.tv_usec)/1000000.0;
}


class MsgRingApp::MessageRingReady : public MessageBase {
public:
    MessageRingReady(MsgRingApp *who) : MessageBase(who) { }
    virtual void run() { _who->doRingReady(); }
};
void
MsgRingApp::ringReady()
{
    enqueueMessage(new MessageRingReady(this));
}
void
MsgRingApp::doRingReady()
{
    _startTime = timeNow();
    for (unsigned i = 0; i < _batchSize; ++i) {
        ++_outstandingMessages;
        _next->passToken(1);
    }
}
