// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <sys/eventfd.h>
#include <unistd.h>
#include <cstdio>
#include <list>

template<class Value>
FDQueue<Value>::FDQueue(FDQueueCallback *queueCb)
{
	_queueFd = eventfd(0, 0);
	_queueCb = queueCb;
}

template<class Value> inline bool
FDQueue<Value>::isEmpty()
{
    _lock.lock();
    bool empty = _queue.empty();
    _lock.unlock();

    return empty;
}

template<class Value> inline void
FDQueue<Value>::enqueue(Value *value)
{
    uint64_t val = 1;
	ssize_t ret;
	_lock.lock();
    _queue.push_back(value);
    _lock.unlock();
	ret = write(_queueFd, &val, sizeof(val));
	if (ret == -1)
		perror("FDQueue::enqueue: write");
}

template<class Value> inline void
FDQueue<Value>::dequeue()
{
	typedef typename std::list<Value *>::iterator Value_it;
	Value *item = NULL;
	std::list<Value *> items;
	uint64_t val;
	ssize_t ret = read(_queueFd, &val, sizeof(val));
	if (ret == -1)
		perror("FDQueue::dequeue: read");
    _lock.lock();
    while (!_queue.empty()) {
        item = _queue.front();
		items.push_back(item);
        _queue.pop_front();
    }
	_lock.unlock();
	for (Value_it it = items.begin(); it != items.end(); it++)
		(*_queueCb)(*it); // callback
}

template<class Value> inline int
FDQueue<Value>::getQueueFd()
{
	return _queueFd;
}
