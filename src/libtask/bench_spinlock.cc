// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "Lock.h"

#include <unistd.h>
#include <cstdlib>
#include <inttypes.h>
#include <ctime>
#include <sys/time.h>
#include <cstring>
#include <iostream>
#include <pthread.h>

class Rand {
private:
    random_data _rdata;
    char _rdata_buf[8];
public:
    Rand() {
        memset(_rdata_buf, 0, sizeof(_rdata_buf));
        memset(&_rdata, 0, sizeof(_rdata));
        initstate_r(time(0) + reinterpret_cast<uint64_t>(this), _rdata_buf,
                    sizeof(_rdata_buf), &_rdata);
    }
    inline unsigned draw(unsigned max) {
        int32_t val;
        random_r(&_rdata, &val);
        return static_cast<unsigned>(val) % max;
    }
};

class ClhLockHandle : public Lock {
private:
    ClhLock *_lock;
    unsigned _index;

public:
    ClhLockHandle(ClhLock *lock, unsigned index) :
        Lock(""), _lock(lock), _index(index) { }
    virtual int lock() {
        return _lock->lock(_index);
    }
    virtual int unlock() {
        return _lock->unlock(_index);
    }
};

int64_t theSharedVar __attribute__ ((aligned (64))) = 0;
Lock *theLock;
ClhLock *chl;
//Mutex theLock;
unsigned numOperations = 10000;
unsigned numThreads = 2;
unsigned lockImpl = 0;



static double
getTime()
{
    timeval tv;
    assert(0 == gettimeofday(&tv, 0));
    double t = tv.tv_sec + tv.tv_usec/1000000.0;
    return t;
}

void *
threadStart(void *arg)
{
    Lock *myLock = static_cast<Lock *>(arg);
    for (unsigned i = 0; i < numOperations; ++i) {
        myLock->lock();
        ++theSharedVar;
        myLock->unlock();
    }
    return reinterpret_cast<void *>(numOperations);
}



int
main(int argc, char *argv[])
{
    char opt;
    while((opt = getopt(argc, argv, "l:o:t:")) > 0) {
        switch (opt) {
        case 'l':
            lockImpl = atoi(optarg);
        case 'o':
            numOperations = atoi(optarg);
            break;
        case 't':
            numThreads = atoi(optarg);
            break;
        default:
            std::cout << "invalid args" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    switch (lockImpl) {
    case 0:
        theLock = new TasLock();
        break;
    case 1:
        theLock = new TtasLock();
        break;
    case 2:
        theLock = new TicketLock();
        break;
    case 3:
        chl = new ClhLock(numThreads);
        break;
    case 4:
        theLock = new AndersonLock(numThreads);
        break;
    case 5:
        theLock = new PthreadMutex();
        break;
    default:
        std::cout << "invalid lock implementation #" << std::endl;
        exit(EXIT_FAILURE);
    };

    // run it
    double startTime = getTime();
    pthread_t threads[numThreads];
    for (unsigned i = 0; i < numThreads; ++i) {
        switch (lockImpl) {
        case 3:
            assert(0 == pthread_create(&threads[i], 0, threadStart,
                                       new ClhLockHandle(chl, i)));
            break;
        default:
            assert(0 == pthread_create(&threads[i], 0, threadStart, theLock));
            break;
        }
    }

    int64_t expected = 0;
    for (unsigned i = 0; i < numThreads; ++i) {
        void *retVal;
        assert(0 == pthread_join(threads[i], &retVal));
        int64_t delta = reinterpret_cast<int64_t>(retVal);
        expected += delta;
    }
    double endTime = getTime();

    //theLock->lock();
    assert(expected == theSharedVar);
    //theLock->unlock();
    delete theLock;

    std::cout << "SpinLock benchmark"
              << "  threads: " << numThreads
              << "  ops_per_thread: " << numOperations
              << "  time: " << endTime-startTime
              << "  rate: " << (numThreads*numOperations)/(endTime-startTime)
              << std::endl;

    return EXIT_SUCCESS;
}
