// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "MsgRingWorker.h"
#include "MsgRingApp.h"

class WorkerFactory : public MsgRingMemberFactory {
public:
    virtual MsgRingMember *operator()() {
        return new MsgRingWorker();
    }
};

class MsgRingWorker::MessageBase : public Message {
protected:
    MsgRingWorker *_who;
public:
    MessageBase(MsgRingWorker *who) : _who(who) { }
};

MsgRingMemberFactory *
MsgRingWorker::getFactory()
{
    static WorkerFactory wf;
    return &wf;
}

class MsgRingWorker::MessageCreateLink : public MessageBase {
protected:
    unsigned _remainingNodes;
    MsgRingMemberFactory *_factory;
    MsgRingApp *_master;
public:
    MessageCreateLink(MsgRingWorker *who,
                      unsigned remainingNodes,
                      MsgRingMemberFactory *factory,
                      MsgRingApp *master) :
        MessageBase(who), _remainingNodes(remainingNodes), _factory(factory),
        _master(master) { }
    virtual void run() {
        _who->doCreateLink(_remainingNodes, _factory, _master);
    }
};
void
MsgRingWorker::createLink(unsigned remainingNodes,
                          MsgRingMemberFactory *factory,
                          MsgRingApp *master)
{
    if (remainingNodes) {
        enqueueMessage(new MessageCreateLink(this, remainingNodes, factory,
                                             master));
    } else {
        _next = master; // note that we are updating state from
                        // outside the process' context, but it's ok
                        // in this case because the process can't
                        // currently be running (it was just created).
        master->ringReady();
    }
}
void
MsgRingWorker::doCreateLink(unsigned remainingNodes,
                            MsgRingMemberFactory *factory,
                            MsgRingApp *master)
{
    _next = (*factory)();
    _next->createLink(remainingNodes-1, factory, master);
}


class MsgRingWorker::MessagePassToken : public MessageBase {
protected:
    unsigned _passes;
public:
    MessagePassToken(MsgRingWorker *who, unsigned passes) :
        MessageBase(who), _passes(passes) { }
    virtual void run() { _who->doPassToken(_passes); }
};
void
MsgRingWorker::passToken(unsigned passes)
{
    enqueueMessage(new MessagePassToken(this, passes));
}
void
MsgRingWorker::doPassToken(unsigned passes)
{
    _next->passToken(passes+1);
}


class MsgRingWorker::MessageShutdown : public MessageBase {
public:
    MessageShutdown(MsgRingWorker *who) : MessageBase(who) { }
    virtual void run() { _who->doShutdown(); }
};
void
MsgRingWorker::shutdown()
{
    enqueueMessage(new MessageShutdown(this));
}
void
MsgRingWorker::doShutdown()
{
    _next->shutdown();
    exit();
}
