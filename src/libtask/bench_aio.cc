// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <cassert>
#include <libaio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <algorithm>
#include <unistd.h>
#include <getopt.h>

enum OpType {
    OP_READ = 0,
    OP_WRITE,
    OP_RW
};

class Timer {
    timeval elapsed;
    timeval startTime;
    bool isStarted;
    uint64_t intervals;
    std::string name;
public:
    Timer(std::string n) {
        name = n;
        elapsed.tv_sec = 0;
        elapsed.tv_usec = 0;
        isStarted = false;
        intervals = 0;
    }
    ~Timer() {
        std::cout << name << ": " << time()
                  << " mean=" << time()/intervals << std::endl;
    }
    void start() {
        assert(!isStarted);
        gettimeofday(&startTime, NULL);
        isStarted = true;
    }
    void stop() {
        assert(isStarted);
        timeval end;
        gettimeofday(&end, NULL);
        if (end.tv_usec < startTime.tv_usec) {
            end.tv_usec += 1000000;
            --end.tv_sec;
        }
        elapsed.tv_usec += end.tv_usec - startTime.tv_usec;
        elapsed.tv_sec += end.tv_sec - startTime.tv_sec;
        while (elapsed.tv_usec > 1000000) {
            elapsed.tv_usec -= 1000000;
            ++elapsed.tv_sec;
        }
        ++intervals;
        isStarted = false;
    }
    double time() {
        assert(!isStarted);
        return elapsed.tv_sec + static_cast<double>(elapsed.tv_usec)/1000000;
    }
};


void
assertRc(int rc, const char *msg)
{
    if (rc < 0) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

int
openFile(std::string filename, bool isDirect, OpType ops)
{
    int flags = isDirect ? O_DIRECT : 0;
    switch (ops) {
    case OP_READ:
        flags |= O_RDONLY;
        break;
    case OP_WRITE:
        flags |= O_WRONLY | O_CREAT;
        break;
    case OP_RW:
        flags |= O_RDWR | O_CREAT;
        break;
    default:
        assert(0);
    }

    int fd = open(filename.c_str(), flags, S_IRUSR | S_IWUSR);
    assertRc(fd, "Opening file:");
    std::cout << "Opened " << filename << " as fd=" << fd
              << " direct=" << isDirect << std::endl;

    return fd;
}

int
setupIoQ(io_context_t *ctx, uint64_t maxOutstanding)
{
    assert(ctx);
    memset(ctx, 0, sizeof(*ctx));
    int rc = io_queue_init(maxOutstanding, ctx);
    assertRc(rc, "io_queue_init:");
    
    return rc;
}

OpType
getOp(OpType allowed)
{
    switch (allowed) {
    case OP_READ:
        return OP_READ;
    case OP_WRITE:
        return OP_WRITE;
    case OP_RW:
        return (rand() < (RAND_MAX>>1)) ? OP_READ : OP_WRITE;
    default:
        assert(0);
    }
}

uint64_t
alignedOffset(uint64_t base, uint64_t alignment)
{
    uint64_t correction = base % alignment;
    if (0 != correction) {
        base += alignment - correction;
    }
    return base;
}


void
doIo(io_context_t *ctx, int fd, OpType opMix, uint64_t alignment,
     uint64_t ioSize, uint64_t maxOutstanding, uint64_t totalIOs, bool doAlign)
{
    Timer tSubmit("io_submit");
    Timer tPoll("epoll_wait");
    Timer tGEvent("io_getevent");
    Timer tEvRead("read eventfd");

    std::cout << "iosize=" << ioSize
              << " alignment=" << alignment
              << " fd=" << fd
              << " max=" << maxOutstanding
              << " total=" << totalIOs
              << " doMemalign=" << doAlign
              << std::endl;

    uint64_t offset = 0;
    uint64_t done = 0;
    uint64_t issued = 0;

    int efd = eventfd(0, EFD_NONBLOCK);

    void *buf[maxOutstanding];
    iocb *iocbs[maxOutstanding];
    for(uint64_t i = 0; i < maxOutstanding; ++i) {
        if (doAlign) {
            assert(0 == posix_memalign(&buf[i], 512, ioSize));
        } else {
            buf[i] = malloc(ioSize);
        }
        iocbs[i] = new iocb;
    }


    // prep initial set
    for (issued = 0; issued < std::min(maxOutstanding, totalIOs); ++issued) {
        OpType op = getOp(opMix);
        offset = alignedOffset(offset, alignment);
        if (OP_READ == op) {
            io_prep_pread(iocbs[issued], fd, buf[issued], ioSize, offset);
            //std::cout << "r" << std::flush;
        } else {
            io_prep_pwrite(iocbs[issued], fd, buf[issued], ioSize, offset);
            //std::cout << "w" << std::flush;
            //std::cout << "w" << ioSize << "@" << offset << std::endl;
        }
        io_set_eventfd(iocbs[issued], efd);
        offset += ioSize;
    }

    // submit them
    tSubmit.start();
    int rc = io_submit(*ctx, issued, iocbs);
    assertRc(rc, "io_submit:");
    tSubmit.stop();

    int epollfd = epoll_create(1);
    assertRc(epollfd, "epoll_create:");
    epoll_event eevent, event_out;

    while (done < totalIOs) {
        // prepare to wait on the event fd
        eevent.events = EPOLLIN;
        eevent.data.fd = efd;
        int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, efd, &eevent);
        assertRc(rc, "epoll_ctl (add):");
        // wait for a completion via epoll
        tPoll.start();
        rc = epoll_wait(epollfd, &event_out, 1, -1);
        assertRc(rc, "epoll_wait:");
        tPoll.stop();
        assert(event_out.data.fd == efd);
        assert(event_out.events == EPOLLIN);
        std::cout << "." << std::flush;
        // de-register
        rc = epoll_ctl(epollfd, EPOLL_CTL_DEL, efd, NULL);
        assertRc(rc, "epoll_ctl (del):");
        // use eventfd to figure out how many I/Os have completed
        uint64_t numReady = 0;
        tEvRead.start();
        rc = read(efd, &numReady, sizeof(numReady));
        assertRc(rc, "read (eventfd):");
        tEvRead.stop();
        assert(numReady > 0);
        std::cout << numReady << std::flush;
        done += numReady;
        // process completed
        uint64_t batch = std::min(static_cast<uint64_t>(10), numReady);
        for (uint64_t i = 0; i < numReady; i += batch) {
            io_event e[batch];
            tGEvent.start();
            uint64_t toGet = std::min(static_cast<uint64_t>(numReady - i),
                                      batch);
            rc = io_getevents(*ctx, toGet, toGet, e, NULL);
            assert(rc == static_cast<int>(toGet));
            tGEvent.stop();
            for (uint64_t j = 0; j < toGet; j++) {
                iocb *cb = e[j].obj;
                void *buf = cb->u.c.buf;
                if (issued < totalIOs) {
                    // re-submit the completed ones
                    OpType op = getOp(opMix);
                    offset = alignedOffset(offset, alignment);
                    if (OP_READ == op) {
                        io_prep_pread(cb, fd, buf, ioSize, offset);
                        //std::cout << "r" << std::flush;
                    } else {
                        io_prep_pwrite(cb, fd, buf, ioSize, offset);
                        //std::cout << "w" << ioSize << "@" << offset << std::endl;
                        //std::cout << "w" << std::flush;
                    }
                    io_set_eventfd(cb, efd);
                    offset += ioSize;
                    //std::cout << "s=" << static_cast<void*>(cb) << std::endl;
                    tSubmit.start();
                    rc = io_submit(*ctx, 1, &cb);
                    assertRc(rc, "io_submit");
                    assert(rc == 1);
                    tSubmit.stop();
                    ++issued;
                }
            }
        }
    }


    for(uint64_t i = 0; i < maxOutstanding; ++i) {
        free(buf[i]);
        delete iocbs[i];
    }

    close(efd);
    std::cout << std::endl;
    std::cout << "final offset: " << offset << std::endl;
}

void
usage(std::string prog)
{
    std::cout << prog
              << " -a <alignment>"
              << " [-A]"
              << " [-d]"
              << " -f <filename>"
              << " -o <optype>"
              << " -p <outstanding>"
              << " -s <iosize>"
              << " -t <total ios>"
              << std::endl
              << "  -a <alignment>    I/O start alignment (bytes) >=1"
              << std::endl
              << "  -A                Align memory buffers @4kB"
              << std::endl
              << "  -d                Use direct I/O (O_DIRECT)"
              << std::endl
              << "  -f <filename>     The test file to write"
              << std::endl
              << "  -o <optype>       Use 'read', 'write', or 'rw' I/Os"
              << std::endl
              << "  -p <outstanding>  Simultaneous I/Os to issue >=1"
              << std::endl
              << "  -s <iosize>       Size of each I/O in bytes >=1"
              << std::endl
              << "  -t <total ios>    Total I/Os to issue >=1"
              << std::endl;
}

int
main(int argc, char *argv[])
{
    std::string filename;
    uint64_t alignment = 0;
    bool useDirect = false;
    OpType opMix = OP_READ;
    uint64_t maxOutstanding = 0;
    uint64_t totalIOs = 0;
    uint64_t ioSize = 0;
    bool doMemalign = false;

    char opt;
    while ((opt = getopt(argc, argv, "Aa:df:ho:p:s:t:")) > 0) {
        switch (opt) {
        case 'A':
            doMemalign = true;
            break;
        case 'a':
            alignment = atoi(optarg);
            break;
        case 'd':
            useDirect = true;
            break;
        case 'f':
            filename = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'o':
            if (0 == strcmp("read", optarg)) {
                opMix = OP_READ;
            } else if (0 == strcmp("write", optarg)) {
                opMix = OP_WRITE;
            } else if (0 == strcmp("rw", optarg)) {
                opMix = OP_RW;
            } else {
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            maxOutstanding = atoi(optarg);
            break;
        case 's':
            ioSize = atoi(optarg);
            break;
        case 't':
            totalIOs = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if ("" == filename ||
        0 == alignment ||
        0 == maxOutstanding ||
        0 == ioSize ||
        0 == totalIOs) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }



    Timer tTotal("total time");
    tTotal.start();
    int fd = openFile(filename, useDirect, opMix);
    io_context_t ctx;
    setupIoQ(&ctx, maxOutstanding);
    doIo(&ctx, fd, opMix, alignment, ioSize, maxOutstanding, totalIOs,
         doMemalign);
    Timer tIod("io_destroy");
    tIod.start();
    assert(0 == io_destroy(ctx));
    tIod.stop();
    Timer tSync("fsync");
    tSync.start();
    fsync(fd);
    tSync.stop();
    close(fd);
    tTotal.stop();

    std::cout << "Bandwidth: " << totalIOs*ioSize/tTotal.time()/(1024*1024)
              << " MB/s" << std::endl;
    std::cout << "done" << std::endl;
    return EXIT_SUCCESS;
}
