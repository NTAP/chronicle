// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "FifoList.h"
#include <cassert>
#include <pthread.h>
#include <inttypes.h>
#include <iostream>
#include <sys/time.h>
#include <deque>
#include <getopt.h>
#include <cstdlib>

class Element : public FifoNode<Element> {
public:
    uint64_t value;
};


static uint64_t threads = 16;
static uint64_t elem_per_thread = 40000000;
static unsigned iterations = 1;

static FifoList<Element> theList;
static std::deque<Element*> theListDeque;
static pthread_mutex_t lock;
static volatile bool done = false;


static void *
producerMutex(void *arg)
{
    uint64_t start = reinterpret_cast<uint64_t>(arg);
    for (uint64_t i = 0; i < elem_per_thread; ++i) {
        Element *e = new Element();
        e->value = ++start;
        pthread_mutex_lock(&lock);
        theListDeque.push_back(e);
        pthread_mutex_unlock(&lock);
    }
    return 0;
}

static void *
consumerMutex(void *arg)
{
    uint64_t total = 0;
    Element *e = 0;
    do {
        e = 0;
        pthread_mutex_lock(&lock);
        if (!theListDeque.empty()) {
            e = theListDeque.front();
            theListDeque.pop_front();
            pthread_mutex_unlock(&lock);
            total += e->value;
            delete e;
        } else {
            pthread_mutex_unlock(&lock);
        }
    } while (e || !done);

    return reinterpret_cast<void *>(total);
}

static void *
producerMalloc(void *arg)
{
    uint64_t start = reinterpret_cast<uint64_t>(arg);
    for (uint64_t i = 0; i < elem_per_thread; ++i) {
        Element *e = new Element();
        e->value = ++start;
        theList.insert(e);
    }
    return 0;
}

static void *
consumerMalloc(void *arg)
{
    uint64_t total = 0;
    Element *e = 0;
    do {
        if ((e = theList.consume())) {
            total += e->value;
            delete e;
        }
    } while (e || !done);

    return reinterpret_cast<void *>(total);
}

static void *
producerPrealloc(void *arg)
{
    Element *e = static_cast<Element *>(arg);
    for (uint64_t i = 0; i < elem_per_thread; ++i) {
        theList.insert(e++);
    }
    return 0;
}

static void *
consumerPrealloc(void *arg)
{
    uint64_t total = 0;
    Element *e = 0;
    do {
        if ((e = theList.consume())) {
            total += e->value;
        }
    } while (!done || e);

    return reinterpret_cast<void *>(total);
}

static double
getTime()
{
    timeval tv;
    assert(0 == gettimeofday(&tv, 0));
    double t = tv.tv_sec + tv.tv_usec/1000000.0;
    return t;
}

static void
mutexTest()
{
    uint64_t total_elements = threads * elem_per_thread;
    pthread_t producers[threads];
    pthread_t consumers[threads];
    pthread_mutex_init(&lock, 0);

    double startTime = getTime();

    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_create(&consumers[i], 0, consumerMutex, 0));
        assert(0 == pthread_create(&producers[i], 0, producerMutex,
                                reinterpret_cast<void*>(i*elem_per_thread)));
    }

    // Reclaim the producers
    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_join(producers[i], 0));
    }

    // allow consumers to exit
    done = true;

    uint64_t totalValue = 0;
    // Reclaim the consumers and check total
    for (uint64_t i = 0; i < threads; ++i) {
        void *retval;
        assert(0 == pthread_join(consumers[i], &retval));
        uint64_t count = reinterpret_cast<uint64_t>(retval);
        totalValue += count;
    }

    double endTime = getTime();
    
    uint64_t expectedValue = total_elements * (total_elements + 1) / 2;
    if (expectedValue != totalValue) {
        std::cout << "Checksum failed..."
                  << " expected=" << expectedValue
                  << " total=" << totalValue
                  << std::endl;
        abort();
    }

    pthread_mutex_destroy(&lock);

    std::cout << "Type: MutexDeque"
              << "  threads: " << threads
              << "  elements_per_thread: " << elem_per_thread
              << "  time: " << endTime-startTime
              << "  rate: " << total_elements/(endTime-startTime)
              << std::endl;
}

static void
mallocTest()
{
    uint64_t total_elements = threads * elem_per_thread;
    pthread_t producers[threads];
    pthread_t consumers[threads];

    double startTime = getTime();

    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_create(&consumers[i], 0, consumerMalloc, 0));
        assert(0 == pthread_create(&producers[i], 0, producerMalloc,
                                reinterpret_cast<void*>(i*elem_per_thread)));
    }

    // Reclaim the producers
    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_join(producers[i], 0));
    }

    // allow consumers to exit
    done = true;

    uint64_t totalValue = 0;
    // Reclaim the consumers and check total
    for (uint64_t i = 0; i < threads; ++i) {
        void *retval;
        assert(0 == pthread_join(consumers[i], &retval));
        uint64_t count = reinterpret_cast<uint64_t>(retval);
        totalValue += count;
    }

    double endTime = getTime();
    
    uint64_t expectedValue = total_elements * (total_elements + 1) / 2;
    if (expectedValue != totalValue) {
        std::cout << "Checksum failed..."
                  << " expected=" << expectedValue
                  << " total=" << totalValue
                  << std::endl;
        abort();
    }

    std::cout << "Type: Malloc"
              << "  threads: " << threads
              << "  elements_per_thread: " << elem_per_thread
              << "  time: " << endTime-startTime
              << "  rate: " << total_elements/(endTime-startTime)
              << std::endl;
}

static void
preallocTest()
{
    uint64_t total_elements = threads * elem_per_thread;
    pthread_t producers[threads];
    pthread_t consumers[threads];

    Element *elements = new Element[total_elements];

    // The elements contain the values from 1..total_elements
    for (uint64_t i = 0; i < total_elements; ++i) {
        elements[i].value = i + 1;
    }

    double startTime = getTime();

    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_create(&consumers[i], 0, consumerPrealloc, 0));
        assert(0 == pthread_create(&producers[i], 0, producerPrealloc,
                                   &elements[i*elem_per_thread]));
    }


    // Reclaim the producers
    for (uint64_t i = 0; i < threads; ++i) {
        assert(0 == pthread_join(producers[i], 0));
    }

    // allow consumers to exit
    done = true;

    uint64_t totalValue = 0;
    // Reclaim the consumers and check total
    for (uint64_t i = 0; i < threads; ++i) {
        void *retval;
        assert(0 == pthread_join(consumers[i], &retval));
        uint64_t count = reinterpret_cast<uint64_t>(retval);
        totalValue += count;
    }

    double endTime = getTime();
    
    uint64_t expectedValue = total_elements * (total_elements + 1) / 2;
    if (expectedValue != totalValue) {
        std::cout << "Checksum failed..."
                  << " expected=" << expectedValue
                  << " total=" << totalValue
                  << std::endl;
        abort();
    }

    delete[] elements;

    std::cout << "Type: Prealloc"
              << "  threads: " << threads
              << "  elements_per_thread: " << elem_per_thread
              << "  time: " << endTime-startTime
              << "  rate: " << total_elements/(endTime-startTime)
              << std::endl;
}

int
main(int argc, char *argv[])
{
    void (*test)() = 0;

    char opt;
    while((opt = getopt(argc, argv, "de:i:mpt:")) > 0) {
        switch (opt) {
        case 'd':
            test = mutexTest;
            break;
        case 'e':
            elem_per_thread = atoi(optarg);
            break;
        case 'i':
            iterations = atoi(optarg);
            break;
        case 'm':
            test = mallocTest;
            break;
        case 'p':
            test = preallocTest;
            break;
        case 't':
            threads = atoi(optarg);
            break;
        default:
            std::cout << "invalid args" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    for (unsigned i = 0; i < iterations; ++i) {
        done = false;
        test();
    }

    exit(EXIT_SUCCESS);
}
