// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef STATGATHERER_H
#define STATGATHERER_H

#include "Process.h"
#include "DataSeries/DataSeriesFile.hpp"
#include "DataSeries/DataSeriesModule.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

class StatGatherer : public Process {
public:
    /// Callback for finish()
    class FinishCb {
    public:
        virtual ~FinishCb() { }
        /// The callback action
        virtual void operator()() = 0;
    };

    StatGatherer(const std::string &dsFilename,
                 int compressionLevel = Extent::compress_all);
    virtual ~StatGatherer();

    /// Add a file's stat info to the log
    /// @param[in] parentIno The inode number of the containing
    /// directory
    /// @param[in] filename The name of the file whose stat info is in
    /// statBuf
    /// @param[in] statBuf The output of the stat() command
    void addFile(ino_t parentIno,
                 const std::string &filename,
                 const struct stat &statBuf);

    /// Add a symlink to the log
    /// @param[in] linkIno The inode number of the symlink
    /// @param[in] filename The full path to the symlink
    void addLink(ino_t linkIno, const std::string &filename);

    /// Called when all files have been added. This causes the process
    /// to finish processing any data and exit.
    /// @param[in] cb Callback that will be called once the process is
    /// finished
    void finish(FinishCb *cb);
private:
    class MessageBase;
    class MessageAddFile;
    class MessageAddLink;
    class MessageFinish;

    void doAddFile(ino_t parentIno,
                   const std::string &filename,
                   struct stat *statBuf);
    void doAddLink(ino_t linkIno, const std::string &filename);
    void doFinish(FinishCb *cb);

    void openSink(std::string filename, int cLevel);

    /// Target size (approximate) for DS extents
    static const unsigned EXTENT_SIZE = 16*1024*1024;
    unsigned _statRecords;
    unsigned _symlinkRecords;

    DataSeriesSink *_dss;
    OutputModule *_omSymlink;
    ExtentSeries _esSymlink;
    /// DataSeries variables for the "stat" extents
    /// @{
    OutputModule *_omStat;
    ExtentSeries _esStat;
    Int64Field _fPIno;
    Variable32Field _fFilename;
    Int64Field _fIno;
    Int64Field _fDev;
    Int32Field _fMode;
    Variable32Field _fType;
    Int64Field _fNLink;
    Int32Field _fUid;
    Int32Field _fGid;
    Int64Field _fRDev;
    Int64Field _fSize;
    Int32Field _fBlkSize;
    Int64Field _fBlocks;
    Int64Field _fATime;
    Int64Field _fMTime;
    Int64Field _fCTime;
    /// @}
    /// DataSeries variables for the "symlink" extents
    /// @{
    Int64Field _fSIno;
    Variable32Field _fSTarget;
    /// @}
};

#endif // STATGATHERER_H

