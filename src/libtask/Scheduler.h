// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "Lock.h"
#include <deque>
#include <list>
#include <pthread.h>

class Process;

/**
 * Represents a task scheduler.
 * A scheduler can execute exactly 1 Process at a time, and a given
 * process will only be executed by a single scheduler at a time.
 */
class Scheduler {
    /// No copy
    Scheduler(Scheduler &);
    /// No assignment
    Scheduler& operator=(const Scheduler &);
public:
    typedef std::list<Scheduler *> SchedList;

    static bool initScheduler();

    Scheduler();
    ~Scheduler();

    /// Start the requested number of schedulers, assigning them to
    /// different cores
    /// @param[in] num The number of schedulers to start
    /// @param[in] initial The initial (runnable) process to enqueue
    /// on a scheduler
    /// @param[in] bindCpu True if the schedulers should be bound to
    /// specific cores or allowed to migrate
    static void startSchedulers(unsigned num,
                                Process *initial,
                                bool bindCpu = true);

    /// Retrieve the number of available processors in the server.
    /// @returns the number of processors
    static unsigned numProcessors();

    /// Set the list and order in which this Scheduler can steal
    /// processes from others. All schedulers must use the same
    /// Semaphore.
    /// @param[in] list Ordered list of Schedulers to steal from
    void setStealList(const SchedList &list);

    /// Retrieve the Scheduler object that is executing the current
    /// code.
    /// @returns the current Scheduler or NULL
    static Scheduler *getCurrentScheduler();

    /// Set the Scheduler object that is executing the current
    /// code. This is only public for testing purposes. Consider it
    /// private in normal programs.
    /// @param[in] s the Scheduler
    static void setCurrentScheduler(Scheduler *s);

    /// Determine whether the currently executing thread context is
    /// actually a Scheduler or some other thread
    /// @returns true if this is a real scheduler
    static bool isInSchedulerContext();

    /// Mark the current context as being a real scheduler (or not).
    /// @param[in] isInScheduler true if this is a real scheduler
    static void setInSchedulerContext(bool isInScheduler);

    /// Enqueues a process on this Scheduler's run queue. The Process
    /// should be runnable (have pending messages).
    /// @param p The Process to enqueue
    void enqueueProcess(Process *p);

    /// Attempt to get the next runnable Process from this Scheduler.
    /// @returns the next available Process or NULL if none are waiting
    Process *getNextProcess();

    /// Run the scheduler.
    /// This is the entry point to start the scheduler. This call will
    /// not return until there are no more processes (0 ==
    /// Process::numProcesses()).
    void run();

    /// Retrieve a pointer to the process that the Scheduler is
    /// currently executing (or NULL)
    /// @returns a pointer to the currently executing Process or NULL
    /// if it is not currently executing a Process
    Process *getCurrentProcess();

private:
    /// Information needed to start a scheduler thread.
    struct SchedulerInfo {
        /// True if the scheduler should be bound to a specific CPU
        bool bind;
        /// The Scheduler that this thread will run
        Scheduler *self;
        /// The cpu to lock the scheduler to
        unsigned whichCpu;
    };

    static const unsigned SCHEDULER_MAGIC = 0xfdb97531;
    unsigned _magicNumber;
    /// Lock protecting the scheduler's run queue
    TtasLock _run_queue_lock;
    /// The scheduler's run queue
    std::deque<Process *> _run_queue;
    /// an eventfd for waiting on work from our neighbor
    int _neighbor_fd;
    static pthread_key_t _currSchedKey;
    static pthread_key_t _isSchedKey;
    static pthread_once_t _currSchedKeyOnce;
    SchedList _stealList;
    /// The Process that the Scheduler is executing
    Process *_currentProcess;
    unsigned _local;
    unsigned _steals;
    unsigned _msgs;

    static void allocSchedKey();
    /// pthread start function.
    /// @param[in] arg A pointer to a SchedulerInfo struct.
    static void *schedThreadStart(void *arg);
    
    /// Try to get the next process within the scheduling group
    /// @returns the next available Process or NULL if none are waiting
    Process *getNextGlobalProcess();
};

#endif // SCHEDULER_H
