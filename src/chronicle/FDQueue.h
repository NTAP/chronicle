// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FDQUEUE_H
#define FDQUEUE_H

#include <Lock.h>
#include <deque>
#include "FDWatcher.h"

/**
 * Implements a thread-safe fifo queue.
 */
template<class Value> class FDQueue {
public:
	class FDQueueCallback {
		public:
			virtual ~FDQueueCallback() { }
			virtual void operator()(Value *val) = 0;
	};

	/**
	 * @param[in] msgCb FDQueue message callback
	 */
    FDQueue(FDQueueCallback *queueCb);

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
     * Remove all elements from the queue and call the corresponding callback.
     * was empty.
     */
    inline void dequeue();
	/**
	 * returns file descriptor corresponding to the queue
	 */
	 inline int getQueueFd();


private:
    FDQueue(FDQueue &);
    FDQueue &operator=(const FDQueue &);

    /// Lock that protects the queue
    TtasLock _lock;
    /// The underlying queue
    std::deque<Value *> _queue;
	/// eventfd used in conjunction with FDWatcher
	int _queueFd;
	/// FDQueue callback
	FDQueueCallback *_queueCb;
	/// FDWatcher callback
	FDWatcher::FdCallback *_fdCb;
};

// because it's a template
#include "FDQueue.cc"

#endif // FDQUEUE_H
