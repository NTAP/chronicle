// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "A2AMaster.h"
#include "A2AWorker.h"
#include "Message.h"
#include <sys/time.h>

class A2AMaster::MessageBase : public Message {
protected:
    A2AMaster *_who;
public:
    MessageBase(A2AMaster *who) : _who(who) { }
};

class A2AMaster::MessageRun : public MessageBase {
protected:
    uint64_t _splits;
    uint64_t _ttl;
public:
    MessageRun(A2AMaster *who, uint64_t splits, uint64_t ttl) :
        MessageBase(who), _splits(splits), _ttl(ttl) { }
    virtual void run() {
        _who->doRun(_splits, _ttl);
    }
};

class A2AMaster::MessageTTLExpired : public MessageBase {
protected:
    uint64_t _hops;
public:
    MessageTTLExpired(A2AMaster *who, uint64_t hops) :
        MessageBase(who), _hops(hops) { }
    virtual void run() {
        _who->doTtlExpired(_hops);
    }
};

A2AMaster::A2AMaster(uint64_t workers, A2ADoneListener *cb)
{
    for (uint64_t i = 0; i < workers; ++i) {
        _workers.push_back(new A2AWorker());
    }
    _cb = cb;
}

void
A2AMaster::run(uint64_t splits, uint64_t ttl)
{
    enqueueMessage(new MessageRun(this, splits, ttl));
}

void
A2AMaster::ttlExpired(uint64_t hops)
{
    enqueueMessage(new MessageTTLExpired(this, hops));
}

void
A2AMaster::doRun(uint64_t splits, uint64_t ttl)
{
    for (std::vector<A2AWorker*>::iterator i = _workers.begin();
         i != _workers.end(); ++i) {
        (*i)->setWorkers(this, _workers);
    }
    _remainingMessages = 1<<splits;
    _startTime = timeNow();
    _workers.front()->passToken(splits, ttl, 1);
}

void
A2AMaster::doTtlExpired(uint64_t hops)
{
    _totalHops += hops;
    if (0 == --_remainingMessages) {
        // all done
        double elapsedTime = timeNow() - _startTime;
        for (std::vector<A2AWorker*>::iterator i = _workers.begin();
             i != _workers.end(); ++i) {
            (*i)->shutdown();
        }
        _cb->a2aDone(_totalHops, elapsedTime);
        exit();
    }
}

double
A2AMaster::timeNow()
{
    timeval tv;
    gettimeofday(&tv, 0);
    double t = tv.tv_sec + tv.tv_usec/1000000.0;
    return t;
}
