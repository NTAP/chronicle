// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef MSGRINGMEMBER_H
#define MSGRINGMEMBER_H

class MsgRingApp;
class MsgRingMember;

/// Factory for creating MsgRingMember objects
class MsgRingMemberFactory {
public:
    virtual ~MsgRingMemberFactory() { }
    virtual MsgRingMember *operator()() = 0;
};

/**
 * Interface: Common operations for all members of the message ring
 */
class MsgRingMember {
public:
    virtual ~MsgRingMember() { }

    /**
     * Build the message ring.
     * This recursively builds the ring, adding nodes until no
     * additional need to be added. The "master" node then becomes
     * this final node's downstream link.
     * @param[in] remainingNodes Remaining number of nodes to add to
     * the ring
     * @param[in] factory Function that creates additional ring
     * members
     * @param[in] master The master link in the ring
     */
    virtual void createLink(unsigned remainingNodes,
                            MsgRingMemberFactory *factory,
                            MsgRingApp *master) = 0;

    /**
     * Pass a token to the next ring member.
     * @param[in] passes The number of times this token has been
     * passed (prior to the current pass)
     */
    virtual void passToken(unsigned passes) = 0;

    /**
     * Propagate a shutdown notice around the ring and cause the
     * process to exit.
     */
    virtual void shutdown() = 0;

protected:
    /// The next node in the ring
    MsgRingMember *_next;
};

#endif // MSGRINGMEMBER_H
