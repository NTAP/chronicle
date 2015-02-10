// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "Process.h"
#include "Lintel/StatsQuantile.hpp"
#include "Message.h"
#include <cassert>

unsigned Process::_alive = 0;
TtasLock Process::_msgPrintLock("MsgPrintLock");

Process::Process(std::string name) :
    _msg_queue_lock("ProcMsgQ"),
    _exited(false), _currScheduler(NULL), _processName(name), _messageLog(0)
{
    __sync_fetch_and_add(&_alive, 1);
#ifdef DEBUG
    _messageLog = new StatsQuantile();
#endif // DEBUG
    _magic = PROCESS_MAGIC;
}

Process::~Process()
{
    _magic = 0;
    if (_messageLog) {
        _msgPrintLock.lock();
        std::cout << "PROCESS MSG STATS: " << _processName << " "
                  << *_messageLog << std::endl;
        _msgPrintLock.unlock();
        delete _messageLog;
    }
    __sync_fetch_and_sub(&_alive, 1);
}

void
Process::assertValid() const
{
    assert(PROCESS_MAGIC == _magic);
}

unsigned
Process::numProcesses()
{
    return _alive;
}

void
Process::enqueueMessage(Message *m, Scheduler *s)
{
    assertValid();
    assert(!hasExited());
    _msg_queue_lock.lock();
    _msg_queue.push_back(m);
    if (NULL == _currScheduler && NULL != s) {
        _currScheduler = s;
        s->enqueueProcess(this);
    }
    _msg_queue_lock.unlock();
}

void
Process::exit()
{
    _exited = true;
    assert(!isRunnable());
    _magic = 0;
}

bool
Process::hasExited() const
{
    return _exited;
}

bool
Process::isRunnable()
{
    _msg_queue_lock.lock();
    bool isEmpty = _msg_queue.empty();
    _msg_queue_lock.unlock();

    return !isEmpty;
}

bool
Process::runOneMessage()
{
    //_currScheduler = Scheduler::getCurrentScheduler();
    Message *m = getNextMessage();
    if (NULL != m) {
        m->run();
        delete m;
        assert(_currScheduler == Scheduler::getCurrentScheduler());
        return true;
    }
    return false;
}

void
Process::requeueProcess()
{
    _msg_queue_lock.lock();
    if (!_msg_queue.empty()) {
        getScheduler()->enqueueProcess(this);
    } else {
        _currScheduler = NULL;
    }
    _msg_queue_lock.unlock();
}

Scheduler *
Process::getScheduler() const
{
    return _currScheduler;
}

Message *
Process::getNextMessage()
{
    Message *m = NULL;
    _msg_queue_lock.lock();
    if (!_msg_queue.empty()) {
        m = _msg_queue.front();
        _msg_queue.pop_front();
    }
    _msg_queue_lock.unlock();
    return m;
}

bool
Process::isInProcessContext()
{
    return (Scheduler::getCurrentScheduler()->getCurrentProcess() == this);
}

void
Process::setOwner(Scheduler *newOwner)
{
    _currScheduler = newOwner;
}

unsigned
Process::pendingMessages()
{
    _msg_queue_lock.lock();
    unsigned msgs = _msg_queue.size();
    _msg_queue_lock.unlock();
    return msgs;
}

const std::string &
Process::processName() const
{
    return _processName;
}

void
Process::logMessageCount()
{
    if (_messageLog) {
        _messageLog->add(pendingMessages());
    }
}
