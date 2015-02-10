// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2014 Netapp, Inc.
 * All rights reserved.
 */

#ifndef RANDCACHE_H
#define RANDCACHE_H

#include "Cache.h"
#include <cstdlib>
#include <map>
#include <vector>

template<class Tag, class Value>
class RandCache: public Cache<Tag, Value> {
    struct Data {
        Tag t;
        Value v;
    };
    typedef std::map<Tag, unsigned> CMap;

    unsigned _maxSize;
    CMap _cacheMap;
    std::vector<Data> _dataVect;

    void evictRandom() {
        if (!_dataVect.empty()) {
            unsigned rIdx = rand() % _dataVect.size();
            remove(_dataVect[rIdx].t);
        }
    }

public:
    RandCache(unsigned maxSize) : _maxSize(maxSize) { }

    virtual void insert(const Tag &t, const Value &v) {
        typename CMap::iterator i = _cacheMap.find(t);
        if (i != _cacheMap.end()) {
            _dataVect[i->second].v = v;
        } else {
            Data d;
            d.t = t;
            d.v = v;
            _dataVect.push_back(d);
            _cacheMap[t] = _dataVect.size() - 1;
            if (_dataVect.size() > _maxSize)
                evictRandom();
        }
    }

    virtual void remove(const Tag &t) {
        typename CMap::iterator i = _cacheMap.find(t);
        if (i == _cacheMap.end())
            return;

        unsigned index = i->second;
        _cacheMap.erase(i);
        if (index < _dataVect.size() - 1) { // need to do swap
            Tag tag = _dataVect.back().t;
            _cacheMap[tag] = index;
            _dataVect[index] = _dataVect.back();
        }
        _dataVect.pop_back();
    }

    virtual Value *lookup(const Tag &t) {
        typename CMap::iterator i = _cacheMap.find(t);
        if (i != _cacheMap.end()) {
            return &(_dataVect[i->second].v);
        } else {
            return 0;
        }
    }
};

#endif // RANDCACHE_H
