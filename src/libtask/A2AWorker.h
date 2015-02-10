// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef A2AWORKER_H
#define A2AWORKER_H

#include "Process.h"
#include <inttypes.h>
#include <vector>
#include <cstdlib>

class A2AMaster;

class A2AWorker : public Process {
public:
    A2AWorker();

    // Should be called exectly once and prior to passing any tokens
    void setWorkers(A2AMaster *master,
                    const std::vector<A2AWorker *> &workers);
    void passToken(uint64_t splits, uint64_t ttl, uint64_t hops);
    void shutdown();

private:
    class MessageBase;
    class MessageToken;
    class MessageShutdown;

    void doPassToken(uint64_t splits, uint64_t ttl, uint64_t hops);
    void doShutdown();
    A2AWorker *randomWorker();

    A2AMaster *_master;
    std::vector<A2AWorker *> _workers;
    random_data _rdata;
    char _rdata_buf[8];
};
#endif // A2AWORKER
