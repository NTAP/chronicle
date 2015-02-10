// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "FDWatcher.h"
#include "TvHelper.h"
#include "Scheduler.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <cassert>
#include <deque>
#include <iostream>

FDWatcher *
FDWatcher::getFDWatcher()
{
    static FDWatcher *fdw = 0;
    if (!fdw)
        fdw = new FDWatcher();
    return fdw;
}

FDWatcher::FDWatcher()
{
    _changeFd = eventfd(0,0);
}

FDWatcher::~FDWatcher()
{
    close(_changeFd);
    while (!_fdEvents.empty()) {
        delete _fdEvents.begin()->second;
        _fdEvents.erase(_fdEvents.begin());
    }
}

void
FDWatcher::registerFdCallback(int fd, Events events, FdCallback *cb)
{
    assert(cb);
    FdEvent *fde = new FdEvent();
    fde->fd = fd;
    fde->which = events;
    fde->cb = cb;

    _fdMutex.lock();
    _fdEvents[fd] = fde;
    _fdMutex.unlock();

    //std::cout << "Registration: fd=" << fd
    //          << " event=" << events << std::endl;
    uint64_t efd_val = 1;
    write(_changeFd, &efd_val, sizeof(efd_val));
}

void
FDWatcher::clearFdCallback(int fd)
{
    FdEvent *fde = NULL;
    _fdMutex.lock();
    FdEventMap::iterator i = _fdEvents.find(fd);
    if (i != _fdEvents.end()) {
        fde = i->second;
        _fdEvents.erase(i);
    }
    _fdMutex.unlock();
    if (fde) {
        delete fde;
        uint64_t efd_val = 1;
        write(_changeFd, &efd_val, sizeof(efd_val));
    }
}

int
FDWatcher::sysSelect(int maxFd, fd_set *readFds, fd_set *writeFds,
                     fd_set *errFds, timeval *timeout)
{
    static const timeval TVZERO = {0, 0};
    timeval *tvp = NULL;
    timeval to;
    if (NULL != timeout) { // don't wait forever
        if (TVZERO != *timeout) {
            timeval now;
            gettimeofday(&now, NULL);
            if (now < *timeout) {
                to = *timeout - now;
            } else {
                to = TVZERO;
            }
        } else {
            to = TVZERO;
        }
        tvp = &to;
    }
    //std::cout << "Starting select" << std::endl;
    return select(maxFd+1, readFds, writeFds, errFds, tvp);
}

unsigned
FDWatcher::mapsToFdSets(int *maxFd, fd_set *readFds, fd_set *writeFds,
                        fd_set *errFds)
{
    assert(maxFd && readFds && writeFds && errFds);
    FD_ZERO(readFds);
    FD_ZERO(writeFds);
    FD_ZERO(errFds);
    *maxFd = -1;
    unsigned numFDs;

    _fdMutex.lock();
    numFDs = _fdEvents.size();
    for (FdEventMap::const_iterator i = _fdEvents.begin();
         i != _fdEvents.end(); ++i) {
        FdEvent *fde = i->second;
        if (fde->which & EVENT_READ) {
            //std::cout << "adding fd=" << fde->fd << " to rdset" << std::endl;
            FD_SET(fde->fd, readFds);
        }
        if (fde->which & EVENT_WRITE) {
            //std::cout << "adding fd=" << fde->fd << " to wrset" << std::endl;
            FD_SET(fde->fd, writeFds);
        }
        if (fde->which & EVENT_ERROR) {
            //std::cout << "adding fd=" << fde->fd << " to errset" << std::endl;
            FD_SET(fde->fd, errFds);
        }
        *maxFd = std::max(*maxFd, fde->fd);
    }
    _fdMutex.unlock();
    return numFDs;
}

void
FDWatcher::waitFds(int schedFd, bool shouldBlock)
{
    fd_set rFd, wFd, eFd;
    int maxFd;
    unsigned numFds = mapsToFdSets(&maxFd, &rFd, &wFd, &eFd);
    FD_SET(schedFd, &rFd);
    FD_SET(schedFd, &eFd);
    maxFd = std::max(maxFd, schedFd);
	FD_SET(_changeFd, &rFd);
    FD_SET(_changeFd, &eFd);
    maxFd = std::max(maxFd, _changeFd);
    int numReady = 0;
    if (shouldBlock) {
        numReady = sysSelect(maxFd, &rFd, &wFd, &eFd, NULL);
    } else {
        if (numFds > 0) { // only poll if we're actually waiting for something
            timeval zero = {0, 0};
            numReady = sysSelect(maxFd, &rFd, &wFd, &eFd, &zero);
        }
    }
    if (numReady > 0) { // dispatch ready descriptors
        std::deque<FdEvent*> readyEvents;

        _fdMutex.lock();
        FdEventMap::iterator i = _fdEvents.begin();
        while (i != _fdEvents.end()) {
            FdEvent *fde = i->second;
            Events e = EVENT_NONE;
            //std::cout << "checking fd=" << i->second->fd << std::endl;
            if (FD_ISSET(fde->fd, &rFd)) { e |= EVENT_READ; }
            if (FD_ISSET(fde->fd, &wFd)) { e |= EVENT_WRITE; }
            if (FD_ISSET(fde->fd, &eFd)) { e |= EVENT_ERROR; }
            if (EVENT_NONE != e) {
                _fdEvents.erase(i);
                fde->which = e;
                readyEvents.push_back(fde);
                // we modified the list, so we need to start over
                i = _fdEvents.begin();
            } else {
                ++i;
            }
        }
        _fdMutex.unlock();

        while (!readyEvents.empty()) {
            FdEvent *fde = readyEvents.front();
            readyEvents.pop_front();
            //std::cout << "callback for fd=" << fde->fd << std::endl;
            (*(fde->cb))(fde->fd, fde->which);
            delete fde;
        }

        // clear change events
        if (FD_ISSET(_changeFd, &rFd)) {
            uint64_t v;
            read(_changeFd, &v, sizeof(v));
        }
        // clear scheduler wake event
        if (FD_ISSET(schedFd, &rFd)) {
            uint64_t v;
            read(schedFd, &v, sizeof(v));
        }
    }
}
