// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "A2AWorker.h"
#include "A2AMaster.h"
#include "Message.h"
#include <cstring>

class A2AWorker::MessageBase : public Message {
protected:
    A2AWorker *_who;
public:
    MessageBase(A2AWorker *who) : _who(who) { }
};

class A2AWorker::MessageToken : public MessageBase {
protected:
    uint64_t _splits;
    uint64_t _ttl;
    uint64_t _hops;
public:
    MessageToken(A2AWorker *who,
                 uint64_t splits,
                 uint64_t ttl,
                 uint64_t hops) :
        MessageBase(who), _splits(splits), _ttl(ttl), _hops(hops) { }
    virtual void run() {
        _who->doPassToken(_splits, _ttl, _hops);
    }
};

class A2AWorker::MessageShutdown : public MessageBase {
public:
    MessageShutdown(A2AWorker *who) : MessageBase(who) { }
    virtual void run() {
        _who->doShutdown();
    }
};

A2AWorker::A2AWorker()
{
    memset(_rdata_buf, 0, sizeof(_rdata_buf));
    memset(&_rdata, 0, sizeof(_rdata));
    initstate_r(time(0) + reinterpret_cast<uint64_t>(this), _rdata_buf,
                sizeof(_rdata_buf), &_rdata);
}

void
A2AWorker::setWorkers(A2AMaster *master,
                      const std::vector<A2AWorker *> &workers)
{
    _master = master;
    _workers = workers;
}

void
A2AWorker::passToken(uint64_t splits, uint64_t ttl, uint64_t hops)
{
    enqueueMessage(new MessageToken(this, splits, ttl, hops));
}

void
A2AWorker::shutdown()
{
    enqueueMessage(new MessageShutdown(this));
}

void
A2AWorker::doPassToken(uint64_t splits, uint64_t ttl, uint64_t hops)
{
    if (splits) {
        A2AWorker *w1 = randomWorker();
        w1->passToken(splits-1, ttl, hops+1);
        A2AWorker *w2 = randomWorker();
        w2->passToken(splits-1, ttl, 1);
    } else if (ttl) {
        A2AWorker *w = randomWorker();
        w->passToken(0, ttl-1, hops+1);
    } else {
        _master->ttlExpired(hops+1);
    }
}

void
A2AWorker::doShutdown()
{
    exit();
}

A2AWorker *
A2AWorker::randomWorker()
{
    int32_t index;
    random_r(&_rdata, &index);
    index = index % _workers.size();
    return _workers[index];
}
