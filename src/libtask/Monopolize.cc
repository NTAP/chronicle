// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "Monopolize.h"
#include "Message.h"

class Monopolize::MessageBase : public Message {
protected:
    Monopolize *_who;
public:
    MessageBase(Monopolize *who) : _who(who) { }
};

class Monopolize::MessageCallback : public MessageBase {
public:
    MessageCallback(Monopolize *who) : MessageBase(who) { }
    virtual void run() {
        _who->doCallback();
    }
};

class Monopolize::MessageShutdown : public MessageBase {
public:
    MessageShutdown(Monopolize *who) : MessageBase(who) { }
    virtual void run() {
        _who->doShutdown();
    }
};

Monopolize::Monopolize() :
    _inShutdown(false)
{
}

void
Monopolize::callback()
{
    enqueueMessage(new MessageCallback(this));
}

void
Monopolize::doCallback()
{
    if (!_inShutdown)
        this->callback();
    else
        exit();
}

void
Monopolize::shutdown()
{
    enqueueMessage(new MessageShutdown(this));
}

void
Monopolize::doShutdown()
{
    _inShutdown = true;
}
