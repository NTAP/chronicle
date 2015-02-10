// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "StatGatherer.h"
#include "Message.h"
#include "SnapshotExtents.h"
#include <iostream>

class StatGatherer::MessageBase : public Message {
protected:
    StatGatherer *_self;
public:
    MessageBase(StatGatherer *self) : _self(self) { }
};

StatGatherer::StatGatherer(const std::string &dsFilename,
                           int compressionLevel) :
    _statRecords(0),
    _symlinkRecords(0),
    _fPIno(_esStat, "p_ino"),
    _fFilename(_esStat, "filename"),
    _fIno(_esStat, "ino"),
    _fDev(_esStat, "dev"),
    _fMode(_esStat, "mode"),
    _fType(_esStat, "ftype"),
    _fNLink(_esStat, "nlink"),
    _fUid(_esStat, "uid"),
    _fGid(_esStat, "gid"),
    _fRDev(_esStat, "rdev"),
    _fSize(_esStat, "size"),
    _fBlkSize(_esStat, "blksize"),
    _fBlocks(_esStat, "blocks"),
    _fATime(_esStat, "atime"),
    _fMTime(_esStat, "mtime"),
    _fCTime(_esStat, "ctime"),
    _fSIno(_esSymlink, "ino"),
    _fSTarget(_esSymlink, "target")
{
    openSink(dsFilename, compressionLevel);
}

StatGatherer::~StatGatherer()
{
    if (_omStat)
        delete _omStat;
    if (_omSymlink)
        delete _omSymlink;
    if (_dss) {
        _dss->flushPending(); // flush so stats are accurate
        DataSeriesSink::Stats s = _dss->getStats();
        s.printText(std::cout);
        delete _dss;
    }
    std::cout << std::endl
              << "Total stat records: " << _statRecords << std::endl
              << "Total symlink records: " << _symlinkRecords << std::endl;
}


class StatGatherer::MessageAddFile : public MessageBase {
    ino_t _parentIno;
    std::string _filename;
    struct stat *_statBuf;
public:
    MessageAddFile(StatGatherer *self, ino_t parentIno,
                   const std::string &filename, struct stat *statBuf) :
        MessageBase(self), _parentIno(parentIno), _filename(filename),
        _statBuf(statBuf) { }
    virtual void run() { _self->doAddFile(_parentIno, _filename, _statBuf); }
};

void
StatGatherer::addFile(ino_t parentIno,
                      const std::string &filename,
                      const struct stat &statBuf)
{
    struct stat *buf = new struct stat;
    memcpy(buf, &statBuf, sizeof(struct stat));
    enqueueMessage(new MessageAddFile(this, parentIno, filename, buf));
}

void
StatGatherer::doAddFile(ino_t parentIno,
                        const std::string &filename,
                        struct stat *statBuf)
{
    /*
    std::cout << "p=" << parentIno
              << " n=" << filename
              << " i=" << statBuf->st_ino
              << std::endl;
    */
    _omStat->newRecord();
    _fPIno.set(parentIno);
    _fFilename.set(filename);
    _fIno.set(statBuf->st_ino);
    _fDev.set(statBuf->st_dev);
    _fMode.set(~S_IFMT & statBuf->st_mode);
    switch(S_IFMT & statBuf->st_mode) {
    case S_IFSOCK:
        _fType.set("socket");
        break;
    case S_IFLNK:
        _fType.set("symlink");
        break;
    case S_IFREG:
        _fType.set("regular");
        break;
    case S_IFBLK:
        _fType.set("blockdev");
        break;
    case S_IFDIR:
        _fType.set("directory");
        break;
    case S_IFCHR:
        _fType.set("chardev");
        break;
    case S_IFIFO:
        _fType.set("fifo");
        break;
    default:
        _fType.set("unknown");
        break;
    }
    _fNLink.set(statBuf->st_nlink);
    _fUid.set(statBuf->st_uid);
    _fGid.set(statBuf->st_gid);
    _fRDev.set(statBuf->st_rdev);
    _fSize.set(statBuf->st_size);
    _fBlkSize.set(statBuf->st_blksize);
    _fBlocks.set(statBuf->st_blocks);
    _fATime.set(statBuf->st_atime);
    _fMTime.set(statBuf->st_mtime);
    _fCTime.set(statBuf->st_ctime);
    ++_statRecords;
    if (_statRecords % 10000 == 0)
        std::cout << "." << std::flush;
    delete statBuf; // allocated in addFile() and passed through the message;
}



class StatGatherer::MessageAddLink : public MessageBase {
    ino_t _linkIno;
    std::string _filename;
public:
    MessageAddLink(StatGatherer *self, ino_t linkIno,
                   const std::string &filename) :
        MessageBase(self), _linkIno(linkIno), _filename(filename) { }
    virtual void run() { _self->doAddLink(_linkIno, _filename); }
};
void
StatGatherer::addLink(ino_t linkIno, const std::string &filename)
{
    enqueueMessage(new MessageAddLink(this, linkIno, filename));
}
void
StatGatherer::doAddLink(ino_t linkIno, const std::string &filename)
{
    const size_t TARGET_LEN = 1025;
    char target[TARGET_LEN];
    memset(target, 0, TARGET_LEN);

    ssize_t bytes = readlink(filename.c_str(), target, TARGET_LEN-1);
    if (bytes < 1) {
        std::cout << "Trouble reading link target: " << filename << std::endl;
        return;
    }

    /*
    std::cout << "i=" << linkIno
              << " t=" << target
              << std::endl;
    */
    _omSymlink->newRecord();
    _fSIno.set(linkIno);
    _fSTarget.set(target);
    ++_symlinkRecords;
}



class StatGatherer::MessageFinish : public MessageBase {
    FinishCb *_cb;
public:
    MessageFinish(StatGatherer *self, FinishCb *cb) :
        MessageBase(self), _cb(cb) { }
    virtual void run() { _self->doFinish(_cb); }
};
void
StatGatherer::finish(FinishCb *cb)
{
    enqueueMessage(new MessageFinish(this, cb));
}
void
StatGatherer::doFinish(FinishCb *cb)
{
    if (cb)
        (*cb)();
    exit();
}

void
StatGatherer::openSink(std::string filename, int cLevel)
{
    ExtentTypeLibrary lib;
    ExtentType::Ptr extStat(lib.registerTypePtr(EXTENT_FSSNAPSHOT_STAT));
    ExtentType::Ptr extSymlink(lib.registerTypePtr(EXTENT_FSSNAPSHOT_SYMLINK));
    _dss = new DataSeriesSink(filename, cLevel);
    _dss->writeExtentLibrary(lib);

    _omStat = new OutputModule(*_dss, _esStat, extStat, EXTENT_SIZE);
    _omSymlink = new OutputModule(*_dss, _esSymlink, extSymlink, EXTENT_SIZE);
}
