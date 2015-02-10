// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FIFOLIST_H
#define FIFOLIST_H

#include <cassert>
#include <atomic>

#define LOCK_PREFIX "lock ; "

// "pause" is supposedly equivalent to "rep;nop"
#define PAUSE()                                                 \
    __asm__ __volatile__("pause"                                \
                         :                                      \
                         :                                      \
                         : "memory")

#define CAS64(ptr, ante, novum, extant)                         \
	__asm__ __volatile__(LOCK_PREFIX "cmpxchgq %1,%2"           \
                         : "=a"(extant)                         \
                         : "q" (novum), "m" (ptr), "0" (ante)   \
                         : "memory")

/*
#define CAS32(ptr, ante, novum, extant)                         \
	__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"           \
                         : "=a"(extant)                         \
                         : "q" (novum), "m" (ptr), "0" (ante)   \
                         : "memory")

#define atomic_sum32(counter, value)                            \
	__asm__ __volatile__ (LOCK_PREFIX "xaddl %%eax, %2"         \
                          : "=a" (value)                        \
                          : "a" (value), "m" (*counter)         \
                          : "memory")                           \

#define atomic_sum64(counter, value)                            \
	__asm__ __volatile__ (LOCK_PREFIX "xaddq %%rax, %2"         \
                          : "=a" (value)                        \
                          : "a" (value), "m" (*counter)         \
                          : "memory")
*/

template<typename T> class FifoList;

/**
 * Base class for structures that can be inserted into a FifoList.
 * The derived class needs to be declared as:
 * @code
 * class MyNode : public FifoNode<MyNode> {
 *   // implementation
 * };
 * @endcode
 */
template<typename T> class FifoNode {
public:
    FifoNode() : _prev(0), _currList(0) { }
    virtual ~FifoNode() { }
    bool isOnList(FifoList<T> *list = 0) const;

private:
    friend class FifoList<T>;
    /// Pointer to the previous node in the FifoList
    //T *volatile _prev;
    std::atomic<T *> _prev;
    /// Pointer to the current list that the node is on. This is used
    /// to detect whether a node gets (erroneously) added to more than
    /// 1 list at a time.
    //FifoList<T> *volatile _currList;
    std::atomic<FifoList<T> *> _currList;
};

/**
 * A lock-free Fifo.
 */
template<typename T> class FifoList {
public:
    FifoList() : _head(0), _tail(0) { }
    /// Insert a node into the fifo.
    /// @param[in] node The node to insert
	inline void insert(T *node);
    /// Remove a node from the fifo.
    /// @returns a pointer to the removed node or NULL if the fifo was
    /// empty
    inline T *consume();
    /// Determine whether the fifo is empty.
    /// @returns true if the FIFO is currently empty
    inline bool isEmpty();

private:
    //T *volatile _head __attribute__ ((aligned (64)));
    //T *volatile _tail __attribute__ ((aligned (64)));
    std::atomic<T *> _head __attribute__ ((aligned (64)));
    std::atomic<T *> _tail __attribute__ ((aligned (64)));
    //Mutex _m;
};



template<typename T>
bool FifoNode<T>::isOnList(FifoList<T> *list) const
{
    if (0 == list) {
        return (0 != _currList);
    } else {
        return (list == _currList);
    }
}



template<typename T>
void FifoList<T>::insert(T *node)
{
    T *next, *extant;

    // ensure the node isn't already on a list
    assert(0 == node->_currList);
    assert(0 == node->_prev);
    node->_currList = this;

    while (true) {
        next = _head;
        CAS64(_head, next, node, extant);
        if (extant == next) {
            /*
             * This is safe because CAS guarantees that only _one_
             * thread will see extant (== head).
             * Note: the current thread no longer owns tenant.
             */
            if (extant == 0) {
                // We own tail if head == tail == 0
                assert(_tail == 0);
                _tail = node;
            } else {
                // setting prev makes a node eligible for consumption
                next->_prev = node;
            }
            break;
        }
        PAUSE();
    }
}

template<typename T>
T *FifoList<T>::consume()
{
    T *node, *extant, *prev;

    while (true) {
        node = _tail;
        /*
         * This is a semantic choice.  The return could be replaced
         * with a continue to make the consume persistent, but this
         * strikes me as dangerous.
         *
         */
        if (0 == node) {
            return 0;
        }

        prev = node->_prev;
        CAS64(_tail, node, prev, extant);
        if (extant == node) {
            // Ensure the node thinks it's on the correct list
            assert(this == node->_currList);

            if (0 == prev) {
                /*
                 * _tail is now null.
                 * If the queue is empty we have to notify insert.  Insert
                 * will only update tail if head == tail == 0.
                 */
                assert(0 == _tail);
                CAS64(_head, node, 0ll, extant);
                if (extant != node) {
                    /*
                     * If the CAS failed then an inserting thread
                     * found us at the head.  That means insert
                     * doesn't think the queue is empty and consume
                     * still owns tail.  We need to fix tail so we we
                     * wait for insert to fill in our prev.
                     */
                    while (0 == node->_prev) {
                        PAUSE();
                    }
                    T *temp = node->_prev;
                    _tail = temp;
                    //_tail = node->_prev;
                }
            }
            break;
        }
        PAUSE();
    }

    node->_prev = 0;
    // Mark the node as no longer being on a list
    node->_currList = 0;
    return node;
}

template<typename T>
bool FifoList<T>::isEmpty()
{
    return (0 == _tail);
}

#endif // FIFOLIST_H
