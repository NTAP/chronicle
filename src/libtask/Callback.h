// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CALLBACK_H
#define CALLBACK_H

/**
 * A Callback is a functor used to send asynchronous responses to
 * operations.
 */
class Callback {
public:
    virtual ~Callback() { }

    /// The operation to be invoked by the callback
    virtual void operator()() = 0;
};

#endif // CALLBACK_H
