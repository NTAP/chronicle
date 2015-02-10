// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2014 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CACHE_H
#define CACHE_H

template<class Tag, class Value>
class Cache {
public:
    virtual ~Cache() { }
    virtual void insert(const Tag &t, const Value &v) = 0;
    virtual void remove(const Tag &t) = 0;
    virtual Value *lookup(const Tag &t) = 0;
};

#endif // CACHE_H
