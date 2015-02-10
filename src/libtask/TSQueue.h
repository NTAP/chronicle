// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef TSQUEUE_H
#define TSQUEUE_H

#include <Lock.h>
#include <deque>

/**
 * Implements a thread-safe fifo queue.
 */
template<class Value> class TSQueue {
public:
    TSQueue();

    /**
     * Determine whether the queue is empty.
     * @returns true iff the queue is empty
     */
    inline bool isEmpty();

    /**
     * Insert an element at the back of the queue.
     * @param[in] value The value to insert
     */
    inline void enqueue(Value *value);

    /**
     * Remove an element from the front of the queue and return it.
     * @returns The front element in the queue or NULL if the queue
     * was empty.
     */
    inline Value *dequeue();

private:
    TSQueue(TSQueue &);
    TSQueue &operator=(const TSQueue &);

    /// Lock that protects the queue
    TtasLock _lock;
    /// The underlying queue
    std::deque<Value *> _queue;
};

// because it's a template
#include "TSQueue.cc"

#endif // TSQUEUE_H
