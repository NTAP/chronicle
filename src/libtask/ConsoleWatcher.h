// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CONSOLEWATCHER_H
#define CONSOLEWATCHER_H

#include "FDWatcher.h"
#include "Process.h"
#include "Monopolize.h"

class ConsoleWatcher : public Process {
public:
    ConsoleWatcher(Monopolize *m);
    ~ConsoleWatcher();

    void callback();
    void start();

private:
    class MessageBase;
    class MessageCallback;
    class MessageStart;

    void doCallback();
    void doStart();

    void registerCallback();

    FDWatcher::FdCallback *_fdc;
    Monopolize *_m;
};

#endif // CONSOLEWATCHER_H
