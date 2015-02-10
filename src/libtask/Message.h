// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef MESSAGE_H
#define MESSAGE_H

class Process;

/**
 * Messages represent the requests/work that are passed between
 * processes.
 */
class Message {
public:
    Message();
    virtual ~Message() { }

    /// Message types should override this function with the actual
    /// actions that should be done as a result of the message.
    virtual void run() = 0;
};

#endif // MESSAGE_H
