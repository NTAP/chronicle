// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DIRGATHERER_H
#define DIRGATHERER_H

#include "Process.h"
#include <string>

class ScannerFactory;

class DirGatherer : public Process {
public:
    /// Operation completion callback
    class CompletionCb {
    public:
        virtual ~CompletionCb() { }
        /// The callback action
        virtual void operator()() = 0;
    };

    /// Create a new directory gatherer.
    /// @param[in] factory Factory class used to generate the actual
    /// directory scanner processes
    DirGatherer(ScannerFactory *factory);
    /// Set the maximum number of directories that will be scanned at
    /// a time.
    /// @param[in] scanners Maximum number of simultaneous directory
    /// scanners
    void maxScanners(unsigned scanners);
    /// Add a directory to be scanned
    /// @param[in] path The pathname to the directory
    void addDirectory(const std::string &path);
    /// Start scanning directories & call the callback when there are
    /// no more directories to process.
    /// @param[in] cb Callback when all directories have been scanned
    void start(CompletionCb *cb);
private:
    class MessageBase;
    class MessageAddDirectory;
    class MessageScannerFinished;
    class MessageStart;
    /// Callback for DirScanner to notify us when it's done
    class ScannerFinishedCb;

    void doAddDirectory(const std::string &path);
    void scannerFinished(ScannerFinishedCb *sf);
    void doScannerFinished();
    void doStart(CompletionCb *cb);

    /// Start scanners up to the max allowable
    void startScanners();

    /// Has the process been started i.e. start() called?
    bool _started;
    /// Factory used to spawn new DirScanner instances
    ScannerFactory *_sfactory;
    /// FIFO list of directories to scan
    std::list<std::string> _paths;
    /// Current number of scanners that are running
    unsigned _outstandingScanners;
    /// Maximum number of scanners to run simultaneously
    unsigned _maxScanners;
    /// Completion callback for the scanner
    CompletionCb *_callback;
};

#endif // DIRGATHERER_H
