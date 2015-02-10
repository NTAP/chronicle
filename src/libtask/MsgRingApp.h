// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef MSGRINGAPP_H
#define MSGRINGAPP_H

#include "Process.h"
#include "Callback.h"
#include "Message.h"
#include "MsgRingMember.h"
#include "MsgRingWorker.h"

/**
 * This is a "message ring" benchmark application.
 * This application provides an indication of the messaging latency
 * between processes by constructing a ring of processes and sending a
 * number of messages around the ring, measuring the time. The
 * available parameters are:
 * - The size of the process ring
 * - The (approximate) total number of message sends
 * - The number of messages to maintain in the ring at a time
 * To start the benchmark, call run(). Results will be returned via
 * the callback.
 *
 * This class not only serves as the entry point for the benchmark,
 * but also as the "master link" in the ring.
 */
class MsgRingApp : public MsgRingMember, public Process {
public:
    /// The benchmark result callback
    class MsgRingResult : public Callback {
    public:
        /// The total elapsed time for sending messages in usecs
        double _elapsedTime;
        /// The total number of messages sent
        unsigned _totalMessages;
    };

    MsgRingApp() : Process("MsgRingApp") { }
    virtual ~MsgRingApp() { }

    virtual void createLink(unsigned remainingNodes,
                            MsgRingMemberFactory *factory,
                            MsgRingApp *master);
    virtual void passToken(unsigned passes);
    virtual void shutdown();

    /**
     * Start the benchmark and notify of completion via a callback.
     * @param[in] loop_size The number of processes in the ring
     * @param[in] messages The approximate number of messages to send
     * @param[in] batch_size The number of messages in the ring at a time
     * @param[inout] cb The result callback
     */
    virtual void run(unsigned loop_size,
                     unsigned messages,
                     unsigned batch_size,
                     MsgRingResult *cb,
                     MsgRingMemberFactory *factory =
                     MsgRingWorker::getFactory());

    /// Notify the "master link" that the ring is ready
    virtual void ringReady();

protected:
    /// Base class for MsgRingApp internal messages
    class MessageBase;
    /// Message to create a link (new node) in the ring
    class MessageCreateLink;
    /// Message to pass a token to the next node
    class MessagePassToken;
    /// Indicates the ring has been constructed and is ready to start
    /// passing tokens
    class MessageRingReady;
    /// Initial application start message
    class MessageRun;
    /// Message to shut down the application
    class MessageShutdown;

    /// Get the current time in fractional seconds since epoch
    static double timeNow();

    void doCreateLink(unsigned remainingNodes,
                      MsgRingMemberFactory *factory,
                      MsgRingApp *master);
    void doPassToken(unsigned passes);
    void doShutdown();
    void doRingReady();
    void doRun(unsigned loop_size,
               unsigned messages,
               unsigned batch_size,
               MsgRingResult *cb,
               MsgRingMemberFactory *factory);

private:
    unsigned _totalMessages;
    unsigned _loopSize;
    unsigned _targetMessages;
    unsigned _batchSize;
    double _startTime;
    MsgRingResult *_cb;
    unsigned _outstandingMessages;
};

#endif // MSGRINGAPP_H
