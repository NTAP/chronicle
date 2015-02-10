// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef MONOPOLIZE_H
#define MONOPOLIZE_H

#include "Process.h"
#include <inttypes.h>
#include <vector>
#include <cstdlib>

class Monopolize : public Process {
public:
    Monopolize();

    void callback();
    void shutdown();

private:
    class MessageBase;
    class MessageCallback;
    class MessageShutdown;

    void doCallback();
    void doShutdown();

    bool _inShutdown;
};

#endif // MONOPOLIZE
