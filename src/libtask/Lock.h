// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef LOCK_H
#define LOCK_H

#include <atomic>
#include <cassert>
#include <string>
#include <pthread.h>

/// Interface class for generic locks.
class Lock {
    Lock(Lock &);
    Lock &operator=(const Lock &);

protected:
    /// Exponential backoff busy-loop.
    /// @param[in] amount The relative amount of time to wait. This is
    /// the exponent in the backoff
    /// @param[in] max The maximum allowable exponent.
    static inline void expBackoff(unsigned amount, unsigned max = 20) {
        for (unsigned i = 1 << std::min(amount, max); i > 0; --i) {
            asm ("pause" : : :);
        }
    }
    /// Linear backoff busy-loop.
    /// @param[in] amount The relative amount of time to wait
    /// @param[in] max The maximum allowed wait time
    static inline void linearBackoff(unsigned amount, unsigned max = 100000) {
        for (unsigned i = std::min(amount, max); i > 0; --i) {
            asm ("pause" : : :);
        }
    }

public:
    /// Create a new lock.
    /// @param[in] name The name of the lock
    Lock(std::string name = "") { }
    virtual ~Lock() { }
    /// Lock it.
    /// @returns 0 on success
    virtual int lock() = 0;
    /// Unlock it.
    /// @returns 0 on success
    virtual int unlock() = 0;
};

/// A lock that supports a trylock operation.
class TryLock : public Lock {
public:
    TryLock(std::string name = "") : Lock(name) { }
    /// Attempt to acquire the lock, and fail if it is contended.
    /// @returns true if the lock was obtained
    virtual bool tryLock() = 0;
    /// Determine whether the lock is currently locked.
    /// @returns true if the lock is currently locked
    virtual bool isLocked() = 0;
};



/// A test-and-set spinlock with exponential backoff
class TasLock : public TryLock {
private:
    std::atomic<bool> _isLocked __attribute__ ((aligned (64)));

    TasLock(TasLock &);
    TasLock &operator=(const TasLock &);

public:
    TasLock(std::string name = "") : TryLock(name), _isLocked(false) { }

    inline int lock() {
        unsigned iters = 3;
        // Test and Set
        while (__atomic_test_and_set(&_isLocked, __ATOMIC_ACQUIRE)) {
            expBackoff(++iters, 25);
        }
        return 0;
    }
    inline bool tryLock() {
        return !__atomic_test_and_set(&_isLocked, __ATOMIC_ACQUIRE);
    }
    inline int unlock() {
        __atomic_clear(&_isLocked, __ATOMIC_RELEASE);
        return 0;
    }
    inline bool isLocked() { return _isLocked; }
};


/// A Test and test-and-set spinlock with exponential backoff
class TtasLock : public TryLock {
private:
    std::atomic<bool> _isLocked __attribute__ ((aligned (64)));

    TtasLock(TtasLock &);
    TtasLock &operator=(const TtasLock &);

public:
    TtasLock(std::string name = "") : TryLock(name), _isLocked(false) { }

    inline int lock() {
        unsigned iters = 0;
        while (__atomic_test_and_set(&_isLocked, __ATOMIC_ACQUIRE)) {
            while (_isLocked) { // wait
                expBackoff(iters++, 20);
            }
        }
        return 0;
    }
    inline bool tryLock() {
        return !__atomic_test_and_set(&_isLocked, __ATOMIC_ACQUIRE);
    }
    inline int unlock() {
        __atomic_clear(&_isLocked, __ATOMIC_RELEASE);
        return 0;
    }
    inline bool isLocked() { return _isLocked; }
};



/// A ticket-based spinlock
class TicketLock : public Lock {
private:
    std::atomic<uint64_t> _nowServing __attribute__ ((aligned (64)));
    std::atomic<uint64_t> _nextAvailable __attribute__ ((aligned (64)));
    
public:
    TicketLock(std::string name = "") : Lock(name), _nowServing(0),
                                        _nextAvailable(0) { }
    inline int lock() {
        unsigned numAhead;
        uint64_t myTicket = _nextAvailable++;
        while ((numAhead = myTicket - _nowServing)) {
            linearBackoff(10*(numAhead-1));
        }
        return 0;
    }
    inline int unlock() {
        ++_nowServing;
        return 0;
    }
};


class AndersonLock : public Lock {
private:
    struct LockElement {
        std::atomic<bool> _mustWait __attribute__ ((aligned (64)));
    };

    std::atomic<unsigned> _nextSlot __attribute__ ((aligned (64)));
    unsigned _holder __attribute__ ((aligned (64)));
    unsigned _slots;
    LockElement *_lockArray;
    
public:
    AndersonLock(unsigned threads, std::string name = "") : Lock(name) {
        _slots = threads;
        _nextSlot = 0;
        _lockArray = new LockElement[_slots];
        for (unsigned i = 0; i < _slots; ++i) {
            _lockArray[i]._mustWait = true;
        }
        _lockArray[0]._mustWait = false;
    }
    ~AndersonLock() {
        delete[] _lockArray;
    }

    inline int lock() {
        unsigned mySlot = _nextSlot++;
        if (0 == mySlot % _slots) {
            _nextSlot -= _slots;
        }
        mySlot = mySlot % _slots;
        while (_lockArray[mySlot]._mustWait) {
            // wait
        }
        _holder = mySlot;
        _lockArray[mySlot]._mustWait = true;
        return 0;
    }
    inline int unlock() {
        unsigned mySlot = _holder;
        _lockArray[(mySlot+1) % _slots]._mustWait = false;
        return 0;
    }
};

class ClhLock {
private:
    struct QNode {
        std::atomic<bool> _succMustWait;
        QNode *_prev;
        QNode() : _succMustWait(false), _prev(0) { }
    } __attribute__ ((aligned (64)));

    QNode *_nodes;
    QNode **_owned;
    std::atomic<QNode *> _lock;

public:
    ClhLock(unsigned maxThreads, std::string name = "") {
        _nodes = new QNode[maxThreads+1];
        _lock = &(_nodes[maxThreads]);
        _owned = new QNode*[maxThreads];
        for (unsigned i = 0; i < maxThreads; ++i) {
            _owned[i] = &(_nodes[i]);
        }
    }

    ~ClhLock() {
        delete[] _nodes;
        delete[] _owned;
    }

    inline int lock(unsigned myIndex) {
        QNode *node = _owned[myIndex];
        node->_succMustWait = true;
        QNode *pred =  _lock.exchange(node);
        node->_prev = pred;
        while (pred->_succMustWait) {
            // wait
        }
        return 0;
    }
    inline int unlock(unsigned myIndex) {
        QNode *node = _owned[myIndex];
        QNode *pred = node->_prev;
        node->_succMustWait = false;
        _owned[myIndex] = pred;
        return 0;
    }
};

class PthreadMutex : public TryLock {
private:
    /// The underlying mutex variable
    pthread_mutex_t _mutex;
public:
    /// Create and initialize the mutex
    PthreadMutex(std::string name = "") : TryLock(name) {
        pthread_mutex_init(&_mutex, 0);
    }
    /// Destroy the mutex
    ~PthreadMutex() {
        assert("Perhaps the Mutex is locked?" &&
               0 == pthread_mutex_destroy(&_mutex));
    }
    inline int lock() { return pthread_mutex_lock(&_mutex); }
    inline int unlock() { return pthread_mutex_unlock(&_mutex); }
    inline bool tryLock() { return 0 == pthread_mutex_trylock(&_mutex); }
    inline bool isLocked() {
        if (pthread_mutex_trylock(&_mutex)) {
            pthread_mutex_unlock(&_mutex);
            return false;
        } else {
            return true;
        }
    };
};

#endif // LOCK_H
