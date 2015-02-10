// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "Scheduler.h"
#include "A2AMaster.h"
#include <getopt.h>
#include <iostream>
#include <cstdlib>

uint64_t workers = 10;
unsigned threads = Scheduler::numProcessors();
uint64_t splits = 10;
uint64_t ttl = 1000;

class ResultPrinter : public A2AMaster::A2ADoneListener {
    virtual void a2aDone(uint64_t totalHops, double elapsedSeconds) {
        std::cout << "A2A"
                  << "  threads: " << threads
                  << "  workers: " << workers
                  << "  splits: " << splits
                  << "  ttl: " << ttl
                  << "  hops: " << totalHops
                  << "  secs: " << elapsedSeconds
                  << "  rate: " << totalHops/elapsedSeconds << " msg/s"
                  << std::endl;
    }
};

int
main(int argc, char *argv[])
{
    char opt;
    while((opt = getopt(argc, argv, "l:s:t:w:")) > 0) {
        switch (opt) {
        case 'l':
            ttl = atoi(optarg);
            break;
        case 's':
            splits = atoi(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        case 'w':
            workers = atoi(optarg);
            break;
        default:
            std::cout << "invalid args" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    Scheduler::initScheduler();
    ResultPrinter rp;
    A2AMaster *m = new A2AMaster(workers, &rp);
    m->run(splits, ttl);
    Scheduler::startSchedulers(threads, m);

    return EXIT_SUCCESS;
}
