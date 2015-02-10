// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "Scheduler.h"
#include "SocketPair.h"
#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <cassert>

class SpCb : public SocketPair::SocketPairResult {
    void operator()() {
        std::cout << "Bytes: " << bytesTransferred << std::endl
                  << "Time: " << elapsedSeconds << std::endl
                  << "Rate: " << bytesTransferred/elapsedSeconds << std::endl
                  << "read calls: " << receiveCalls << std::endl
                  << "write calls: " << sendCalls << std::endl;
    }
};


int
main(int argc, char *argv[])
{
    unsigned bytes = 1000000;
    unsigned threads = Scheduler::numProcessors();

    char opt;
    while ((opt = getopt(argc, argv, "b:t:")) > 0) {
        switch (opt) {
        case 'b':
            bytes = atoi(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        default:
            assert(0);
        }
    }


    Scheduler::initScheduler();
    SocketPair *sp = new SocketPair();
    SpCb result;
    sp->run(bytes, &result);
    Scheduler::startSchedulers(threads, sp);

    std::cout << "all done." << std::endl;

    return 0;
}
