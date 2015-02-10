// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "FifoList.h"

#include <unistd.h>
#include <cstdlib>
#include <inttypes.h>
#include <ctime>
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

class Node : public FifoNode<Node> { };
typedef FifoList<Node> NodeList;

NodeList *theLists;
unsigned numLists = 10;
unsigned numSwaps = 100;
unsigned numThreads = 2;
unsigned numPreloadEntries = 5;
unsigned numTempSlots = 4;



void *
threadStart(void *arg)
{
    // set up the temp slots
    Node *temps[numTempSlots];
    for (unsigned i = 0; i < numTempSlots; ++i) {
        temps[i] = 0;
    }

    Rand rg;

    for (unsigned i = 0; i < numSwaps; ++i) {
        unsigned slot = rg.draw(numTempSlots);
        unsigned list = rg.draw(numLists);
        if (0 == temps[slot]) {
            temps[slot] = theLists[list].consume();
        } else {
            theLists[list].insert(temps[slot]);
            temps[slot] = 0;
        }
    }

    return 0;
}



int
main(int argc, char *argv[])
{
    char opt;
    while((opt = getopt(argc, argv, "l:n:p:s:t:")) > 0) {
        switch (opt) {
        case 'l':
            numLists = atoi(optarg);
            break;
        case 'n':
            numSwaps = atoi(optarg);
            break;
        case 'p':
            numPreloadEntries = atoi(optarg);
            break;
        case 's':
            numTempSlots = atoi(optarg);
            break;
        case 't':
            numThreads = atoi(optarg);
            break;
        default:
            std::cout << "invalid args" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    theLists = new NodeList[numLists];

    // preload the lists
    for (unsigned j = 0; j < numLists; ++j) {
        for (unsigned i = 0; i < numPreloadEntries; ++i) {
            theLists[j].insert(new Node);
        }
    }

    // run it
    pthread_t threads[numThreads];
    for (unsigned i = 0; i < numThreads; ++i) {
        assert(0 == pthread_create(&threads[i], 0, threadStart, 0));
    }
    for (unsigned i = 0; i < numThreads; ++i) {
        assert(0 == pthread_join(threads[i], 0));
    }

    // clean up the lists
    for (unsigned i = 0; i < numLists; ++i) {
        while (Node *n = theLists[i].consume()) {
            delete n;
        }
    }
    delete[] theLists;

    std::cout << "done." << std::endl;

    return EXIT_SUCCESS;
}
