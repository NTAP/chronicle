// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FDWATCHER_H
#define FDWATCHER_H

#include "Lock.h"
#include <map>
#include <sys/time.h>
#include <pthread.h>

class Scheduler;

/**
 * Singleton object that allows working with asynchronous events
 * (currently just file descriptors).
 * Get a handle to the watcher by
 * calling getFDWatcher(). New registrations can then be added by
 * calling registerFdCallback(), providing the (non-blocking) file
 * descriptor, the events to wait for, and the callback function to
 * use to notify. When the descriptor becomes ready, the callback will
 * be triggered and the registration will be removed.
 */
class FDWatcher {
    /// No copy
    FDWatcher(FDWatcher &);
    /// No assignment
    FDWatcher& operator=(const FDWatcher &);

public:
    /// Events that could happen or that a requestor may be interested
    /// in.
    typedef unsigned Events;
    static const Events EVENT_NONE  = 0x00; ///< no event
    static const Events EVENT_READ  = 0x01; ///< fd is ready for reading
    static const Events EVENT_WRITE = 0x02; ///< fd is ready for writing
    static const Events EVENT_ERROR = 0x04; ///< error on fd

    /// Get a handle to the FDWatcher singleton
    /// @returns a pointer to the FDWatcher object
    static FDWatcher *getFDWatcher();

    /// Callback 
    class FdCallback {
    public:
        virtual ~FdCallback() { }
        /// Callback function for file descriptor events.
        /// @param[in] the file descriptor that is ready
        /// @param[in] the events that are ready on that descriptor
        virtual void operator()(int fd, Events event) = 0;
    };

    virtual ~FDWatcher();

    /// Register to be notified about Events on a file
    /// descriptor. Once an event is detected and the callback
    /// triggered, users will need to re-register (after handling the
    /// triggering event).
    /// @param[in] fd the file descriptor of interest (should be
    /// marked O_NONBLOCK)
    /// @param[in] events ORed set of flags indicating the events to
    /// watch for
    /// @param[in] cb The callback to call when an event is detected
    void registerFdCallback(int fd, Events events, FdCallback *cb);
    /// Unregister from a file descriptor. This removes a registration
    /// that was created by regasterFdCallback().
    /// @param[in] fd the file descriptor to unregister
    void clearFdCallback(int fd);

    /// Wait for a file descriptor to become ready, including the
    /// scheduler's fd. THIS SHOULD ONLY BE CALLED BY THE SCHEDULER.
    /// @param[in] schedFd The scheduler's file descriptor.
    /// @param[in] shouldBlock true if this function should wait
    void waitFds(int schedFd, bool shouldBlock);

protected:
    FDWatcher();

    /// Call the system's select() function to wait for an event.
    /// @param[in] maxFd the highest numbered descriptor in the
    /// supplied fd_sets
    /// @param[inout] readFds the file descriptors that should be
    /// monitored for reading. Upon return, it will contain the actual
    /// descriptors that are ready
    /// @param[inout] readFds the file descriptors that should be
    /// monitored for writing. Upon return, it will contain the actual
    /// descriptors that are ready
    /// @param[inout] readFds the file descriptors that should be
    /// monitored for exceptions. Upon return, it will contain the
    /// actual descriptors that have had an exception event
    /// @param[in] timeout The time until which to wait, assuming no
    /// descriptors are ready. Note: The value of timeout is undefined
    /// after calling sysSelect.
    /// @returns the number of descriptors that are ready
    int sysSelect(int maxFd, fd_set *readFds, fd_set *writeFds,
                  fd_set *errFds, timeval *timeout);

    /// Generates the fd_sets based on the registration data structure
    /// @param[out] maxFd the highest numbered file descriptor in any
    /// set
    /// @param[out] readFds descriptors to watch for read availability
    /// @param[out] writeFds descriptors to watch for write availability
    /// @param[out] errFds descriptors to watch for error conditions
    /// @returns the number of fds (not including the changeFd) that
    /// are in the sets
    unsigned mapsToFdSets(int *maxFd, fd_set *readFds, fd_set *writeFds,
                          fd_set *errFds);

private:
    /// Describes a file descriptor watch registration
    struct FdEvent {
        int fd;         ///< the descriptor
        Events which;   ///< the events to watch for
        FdCallback *cb; ///< the associated callback to use
    };

    /// Data structure for tracking registrations
    typedef std::map<int, FdEvent *> FdEventMap;

    PthreadMutex _fdMutex;      ///< Protects _fdEvents
    FdEventMap _fdEvents;       ///< List of registrations
    int _changeFd;              ///< Allows notifying select loop of
                                ///< changes to the registration list
};

#endif // FDWATCHER_H
