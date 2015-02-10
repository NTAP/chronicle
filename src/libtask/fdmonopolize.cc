// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "ConsoleWatcher.h"
#include "Monopolize.h"
#include "Scheduler.h"

int
main(int argc, char *argv[])
{
    unsigned threads = Scheduler::numProcessors();


    Scheduler::initScheduler();
    Monopolize *m = new Monopolize();
    ConsoleWatcher *c = new ConsoleWatcher(m);
    c->start();
    Scheduler::startSchedulers(threads, c);
    
    return 0;
}
