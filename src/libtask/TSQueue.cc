// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

template<class Value>
TSQueue<Value>::TSQueue()
{
    // empty
}

template<class Value> inline bool
TSQueue<Value>::isEmpty()
{
    _lock.lock();
    bool empty = _queue.empty();
    _lock.unlock();

    return empty;
}

template<class Value> inline void
TSQueue<Value>::enqueue(Value *value)
{
    _lock.lock();
    _queue.push_back(value);
    _lock.unlock();
}

template<class Value> inline Value *
TSQueue<Value>::dequeue()
{
    Value *v = NULL;
    _lock.lock();
    if (!_queue.empty()) {
        v = _queue.front();
        _queue.pop_front();
    }
    _lock.unlock();

    return v;
}
