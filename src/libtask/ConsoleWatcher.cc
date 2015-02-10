// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "ConsoleWatcher.h"
#include "Message.h"
#include <iostream>
#include <unistd.h>

class FdCb : public FDWatcher::FdCallback {
    ConsoleWatcher *_c;
public:
    FdCb(ConsoleWatcher *c) : _c(c) { }
    void operator()(int fd, FDWatcher::Events event) {
        assert(0 == fd);
        assert(event == FDWatcher::EVENT_READ);
        _c->callback();
    }
};

ConsoleWatcher::ConsoleWatcher(Monopolize *m) :
    _m(m)
{
    _fdc = new FdCb(this);
}

ConsoleWatcher::~ConsoleWatcher()
{
    delete _fdc;
}

class ConsoleWatcher::MessageBase : public Message {
protected:
    ConsoleWatcher *_who;
public:
    MessageBase(ConsoleWatcher *who) : _who(who) { }
};

class ConsoleWatcher::MessageCallback : public MessageBase {
public:
    MessageCallback(ConsoleWatcher *who) : MessageBase(who) { }
    virtual void run() {
        _who->doCallback();
    }
};

class ConsoleWatcher::MessageStart : public MessageBase {
public:
    MessageStart(ConsoleWatcher *who) : MessageBase(who) { }
    virtual void run() {
        _who->doStart();
    }
};

void
ConsoleWatcher::callback()
{
    enqueueMessage(new MessageCallback(this));
}

void
ConsoleWatcher::doCallback()
{
    char byte = 0;
    assert(1 == read(0, &byte, 1));
    switch (byte) {
    case 's':
        _m->callback();
        break;
    case 'q':
        _m->shutdown();
        exit();
        break;
    default:
        std::cout << byte << std::endl;
    };

    if (!hasExited())
        registerCallback();
}

void
ConsoleWatcher::start()
{
    enqueueMessage(new MessageStart(this));
}

void
ConsoleWatcher::doStart()
{
    registerCallback();
}

void
ConsoleWatcher::registerCallback()
{
    std::cout << "setting up cb" << std::endl;
    FDWatcher::getFDWatcher()->registerFdCallback(0, FDWatcher::EVENT_READ,
                                                  _fdc);
}
