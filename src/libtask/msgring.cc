// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "MsgRingApp.h"
#include "Scheduler.h"
#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <cassert>

class MsgRingCb : public MsgRingApp::MsgRingResult {
    void operator()();
};

static unsigned threads;
static unsigned batch;
static unsigned loop_size;

void
MsgRingCb::operator()()
{
    std::cout << "Msgring"
              << "  threads: " << threads
              << "  loopsize: " << loop_size
              << "  messages: " << _totalMessages
              << "  secs: " << _elapsedTime
              << "  rate: "
              << (static_cast<double>(_totalMessages)/_elapsedTime)
              << " msg/s"
              << std::endl;
}

int
main(int argc, char *argv[])
{
    loop_size = 1000;
    batch = 1;
    threads = Scheduler::numProcessors();
    unsigned messages = 50000;

    char opt;
    while ((opt = getopt(argc, argv, "b:l:m:t:")) > 0) {
        switch (opt) {
        case 'b':
            batch = atoi(optarg);
            break;
        case 'l':
            loop_size = atoi(optarg);
            break;
        case 'm':
            messages = atoi(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        default:
            assert(0);
        }
    }


    Scheduler::initScheduler();
    MsgRingApp *app = new MsgRingApp;
    MsgRingCb res;
    app->run(loop_size, messages, batch, &res);
    Scheduler::startSchedulers(threads, app);

    return EXIT_SUCCESS;
}
