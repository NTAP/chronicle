// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "Lintel/Stats.hpp"
#include "Lock.h"
#include "Scheduler.h"
#include <deque>
#include <atomic>
#include <string>

class Message;

/**
 * Defines the base class for a process.
 */
class Process {
    /// No copy
    Process(Process &);
    /// No assignment
    Process& operator=(const Process &);
    static const unsigned PROCESS_MAGIC = 0x19283746;

public:
    Process(std::string name = "unnamed");
    virtual ~Process();
	
	/// Add a Message to this Process queue.
    /// @param[in] m The Message to enqueue
    /// @param[in] s The Scheduler to put this process on if not
    /// already queued
    void enqueueMessage(Message *m, Scheduler *s =
                        Scheduler::getCurrentScheduler());

    /// Get the number of processes that are alive.
    /// @returns the number of Process objects in existence
    static unsigned numProcesses();

    /// Mark a process as "exited."
    void exit();

    /// Determine if this Process is finished and ready to be freed.
    /// @returns true if it can be freed
    bool hasExited() const;

    /// Determine whether this Process is runnable (has any pending
    /// Message).
    /// @returns true if there are pending Message
    bool isRunnable();

    /// Retrieve the number of messages currently on the process'
    /// message queue.
    /// @returns the number of pending messages.
    unsigned pendingMessages();

    /// Run one message from the Process message queue.
    /// @returns true if a Message was executed
    bool runOneMessage();

    /// Re-queue the process on the current Scheduler if it is still
    /// runnable
    void requeueProcess();

    /// Get the current Scheduler associated with this process
    Scheduler *getScheduler() const;

    /// Assert that the Process is properly initialized
    void assertValid() const;

    /// Determine if this process is the one being executed by the
    /// Scheduler.
    /// @returns true if the Scheduler is running this Process
    bool isInProcessContext();

    /// Changes the process' owning scheduler.
    /// This should only be used by the Scheduler!
    /// @param[in] newOwner The new owning Scheduler
    void setOwner(Scheduler *newOwner);

    /// Get the name of the process
    const std::string &processName() const;

    /// Record the current count of pending messages
    void logMessageCount();

	/// Returns a unique string that identifies the process
	virtual std::string getId() 
		{ return std::to_string(reinterpret_cast<uint64_t>(this)); }

protected:

private:
    /// Get the next Message to execute for this Process.
    /// @returns a Message to execute or NULL if there are none.
    Message *getNextMessage();

    /// The number of processes that are currently alive (i.e., the
    /// number of Process objects that are allocated). This variable
    /// should be updated atomically via __sync_fetch_and_add().
    static unsigned _alive;

    /// Lock protecting the message queue and the current scheduler ptr
    TtasLock _msg_queue_lock;

    /// Queue of messages for this process
    std::deque<Message *> _msg_queue;

    /// True if the process has exited. Note that only transitions
    /// from false -> true.
    bool _exited;

    /// The scheduler this is currently executing this Process
    std::atomic<Scheduler *> _currScheduler;

    /// Text name of the process
    std::string _processName;

    Stats *_messageLog;
    static TtasLock _msgPrintLock;

    unsigned _magic;
};

#endif // PROCESS_H
