// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <atomic>
#include <algorithm>

class Semaphore {
private:
    Semaphore(Semaphore &);
    Semaphore &operator=(const Semaphore &);
public:
    Semaphore() { }
    virtual ~Semaphore() { }
    virtual void up() = 0;
    virtual void down() = 0;
};



class PosixSemaphore : public Semaphore {
private:
    sem_t _sem;
public:
    explicit PosixSemaphore(unsigned initialValue) {
        sem_init(&_sem, 0, initialValue);
    }
    ~PosixSemaphore() { sem_destroy(&_sem); }

    inline void up() { sem_post(&_sem); }
    inline void down() { sem_wait(&_sem); }
};



class SpinSemaphore : public Semaphore {
private:
    std::atomic<unsigned> _count;
    static inline void expBackoff(unsigned amount, unsigned max = 30) {
        for (unsigned i = 1 << std::min(amount, max); i > 0; --i) {
            asm ("pause" : : :);
        }
    }
public:
    explicit SpinSemaphore(unsigned initialValue) : _count(initialValue) {
    }
    inline void up() {
        ++_count;
    }
    inline void down() {
        unsigned iters = 10;
        unsigned current;
        do {
            while (0 == (current = _count)) {
                expBackoff(++iters);
            }
        } while (!std::atomic_compare_exchange_strong(&_count,
                                                      &current, current-1));
    }
};



class CvSemaphore : public Semaphore {
private:
    pthread_cond_t _cv;
    pthread_mutex_t _m;
    int _count;
public:
    explicit CvSemaphore(unsigned initialValue) : _count(initialValue) {
        pthread_cond_init(&_cv, 0);
        pthread_mutex_init(&_m, 0);
    }
    ~CvSemaphore() {
        pthread_mutex_destroy(&_m);
        pthread_cond_destroy(&_cv);
    }
    inline void up() {
        pthread_mutex_lock(&_m);
        if (_count++ <= 0) {
            pthread_cond_signal(&_cv);
        }
        pthread_mutex_unlock(&_m);
    }
    inline void down() {
        pthread_mutex_lock(&_m);
        if (_count-- <= 0) {
            pthread_cond_wait(&_cv, &_m);
        }
        pthread_mutex_unlock(&_m);
    }
};

#endif // SEMAPHORE_H
