// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DIRSCANNER_H
#define DIRSCANNER_H

#include "Process.h"
#include <string>
#include <dirent.h>

class DirGatherer;
class StatGatherer;

/**
 * This process reads a directory, stat()s each entry, and provides
 * the result to downstream Process. All entries go to the
 * StatGatherer. Directories also go to the DirGatherer.
 */
class DirScanner : public Process {
    /// No copy
    DirScanner(DirScanner &);
    /// No assignment
    DirScanner &operator=(const DirScanner &);
public:
    /// Callback that will be called when this process has finished
    /// scanning its assigned directory
    class CompletionCb {
    public:
        /// The DirScanner that has finished
        DirScanner *scanner;
        virtual ~CompletionCb() { }
        virtual void operator()() = 0;
    };

    /// Scan the associated directory, passing the entries downstream.
    /// @param[in] directory Full path to the directory to scan
    /// @param[in] sg Downstream process to send entries to
    /// @param[in] lg Downstream process to send symlinks to
    /// @param[in] dg Downstream process to send directories to
    /// @param[in] cb A calback that will be called when the directory
    /// has been fully processed
    DirScanner(const std::string &directory,
               StatGatherer *sg,
               DirGatherer *dg,
               CompletionCb *cb);
    virtual ~DirScanner();
protected:
    /// Opens the directory, associating _dir with _path
    /// @returns Zero on success or an errno error code on failure
    virtual int sysOpenDir();
    /// Puts the next directory entry into _entry
    /// @returns true if an entry was successfully fetched
    virtual bool sysReadDir();
    /// Closes the directory
    virtual void sysCloseDir();

private:
    /// Base class for internal messages
    class MessageBase;
    class MessageOpenDir;
    class MessageProcessEntry;

    void openDirectory();
    void processEntry();
    void doOpenDirectory();
    void doProcessEntry();

    /// Handle to the directory being scanned
    DIR *_dir;
    /// Pathname of the directory being scanned
    std::string _path;
    /// Downstream recipient of entries
    StatGatherer *_sg;
    /// Downstream recipient of directories
    DirGatherer *_dg;
    /// Callback to use when scanning is complete
    CompletionCb *_cb;
    dirent *_currentEntry;
    /// Inode # of the directory being scanned
    ino_t _dirIno;
};

#endif // DIRWALK_H
