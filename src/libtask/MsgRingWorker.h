// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef MSGRINGWORKER_H
#define MSGRINGWORKER_H

#include "MsgRingMember.h"
#include "Process.h"

/**
 * The generic "worker" nodes in the message ring.
 */
class MsgRingWorker: public MsgRingMember, public Process {
public:
    MsgRingWorker() : Process("MsgRingWorker") { }
    virtual ~MsgRingWorker() { }

    virtual void createLink(unsigned remainingNodes,
                            MsgRingMemberFactory *factory,
                            MsgRingApp *master);
    virtual void passToken(unsigned passes);
    virtual void shutdown();

    /// A MsgRingMemberFactory that creates MsgRingWorkers
    static MsgRingMemberFactory *getFactory();

protected:
    class MessageBase;
    class MessageCreateLink;
    class MessagePassToken;
    class MessageShutdown;

    void doCreateLink(unsigned remainingNodes,
                      MsgRingMemberFactory *factory,
                      MsgRingApp *master);
    void doPassToken(unsigned passes);
    void doShutdown();
};

#endif // MSGRINGWORKER_H
