// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef A2AMASTER_H
#define A2AMASTER_H

#include "Process.h"
#include <inttypes.h>
#include <vector>

class A2AWorker;

class A2AMaster : public Process {
public:
    class A2ADoneListener {
    public:
        virtual ~A2ADoneListener() { }
        virtual void a2aDone(uint64_t totalHops, double elapsedSeconds) = 0;
    };

    A2AMaster(uint64_t workers, A2ADoneListener *cb);
    void run(uint64_t splits, uint64_t ttl);
    void ttlExpired(uint64_t hops);

private:
    class MessageBase;
    class MessageRun;
    class MessageTTLExpired;

    void doRun(uint64_t splits, uint64_t ttl);
    void doTtlExpired(uint64_t hops);
    double timeNow();

    std::vector<A2AWorker *> _workers;
    uint64_t _totalHops;
    uint64_t _remainingMessages;
    A2ADoneListener *_cb;
    double _startTime;
};
#endif //A2AMASTER_H
