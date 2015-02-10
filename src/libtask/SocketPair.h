// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef SOCKETPAIR_H
#define SOCKETPAIR_H

#include "Process.h"
#include "Callback.h"

/**
 * Implements a process to test the FDWatcher.  This process creates a
 * pipe (pipe2()) and transfers a fixed number of bytes through it.
 */
class SocketPair : public Process {
public:
    class SocketPairResult : public Callback {
    public:
        SocketPairResult() : elapsedSeconds(0.0), sendCalls(0),
                             receiveCalls(0), bytesTransferred(0) { }
        /// Total time to transfer the data
        double elapsedSeconds;
        /// Number of times write() was called
        unsigned sendCalls;
        /// Number of times read() was called
        unsigned receiveCalls;
        /// Total bytes transfered
        unsigned bytesTransferred;
    };

    SocketPair();
    ~SocketPair();

    /// Run the socket pair test
    /// @param[in] bytes the number of bytes to transfer
    /// @param[in] cb callback used to notify the caller that the
    /// transfer is complete (and return the results). May not be NULL
    void run(unsigned bytes, SocketPairResult *cb);

protected:
    class MessageBase;
    class MessageRun;
    class MessageSend;
    class MessageReceive;

    class ReadReadyCb;
    class WriteReadyCb;

    void send();
    void receive();

    void doRun(unsigned bytes, SocketPairResult *cb);
    void doSend();
    void doReceive();

private:
    char *_srcBuf;             ///< Source data buffer (write() from)
    char *_dstBuf;             ///< Receive data buffer (read() into)
    unsigned _totalBytes;      ///< Total bytes to transfer
    unsigned _sentBytes;       ///< Bytes sent so far
    unsigned _receivedBytes;   ///< Bytes received so far
    int _sockpair[2];          ///< The pipe ends through which to transfer
    ReadReadyCb *_rcb;         ///< FDWatcher callback when ready for reading
    WriteReadyCb *_wcb;        ///< FDWatcher callback when ready for writing
    SocketPairResult *_result; ///< Main process callback when done
};

#endif // SOCKETPAIR_H
