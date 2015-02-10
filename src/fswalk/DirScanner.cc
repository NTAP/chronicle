// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "DirScanner.h"
#include "DirGatherer.h"
#include "StatGatherer.h"
#include "Message.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

DirScanner::DirScanner(const std::string &directory,
                       StatGatherer *sg,
                       DirGatherer *dg,
                       CompletionCb *cb) :
    _dir(NULL), _path(directory), _sg(sg), _dg(dg), _cb(cb)
{
    //ssize_t len = offsetof(dirent, d_name) +
    //    pathconf(_path.c_str(), _PC_NAME_MAX) + 1;
    _currentEntry = new dirent;
    openDirectory();
}

DirScanner::~DirScanner()
{
    delete _currentEntry;
    sysCloseDir();
}

int
DirScanner::sysOpenDir()
{
    struct stat s;
    stat(_path.c_str(), &s);
    _dirIno = s.st_ino;
    _dir = opendir(_path.c_str());
    if (!_dir)
        return errno;
    else 
        return 0;
}

bool
DirScanner::sysReadDir()
{
    assertValid();
    dirent *cep;
    int rc = readdir_r(_dir, _currentEntry, &cep);
    if (0 != rc || cep != _currentEntry)
        return false;
    else
        return true;
}

void
DirScanner::sysCloseDir()
{
    closedir(_dir);
}

class DirScanner::MessageBase : public Message {
protected:
    DirScanner *_self;
public:
    MessageBase(DirScanner *self) : _self(self) { }
};



class DirScanner::MessageOpenDir : public MessageBase {
public:
    MessageOpenDir(DirScanner *self) : MessageBase(self) { }
    virtual void run() { _self->doOpenDirectory(); }
};
void
DirScanner::openDirectory()
{
    assertValid();
    enqueueMessage(new MessageOpenDir(this));
}
void
DirScanner::doOpenDirectory()
{
    if (0 == sysOpenDir()) {
        processEntry();
    } else {
        std::cerr << "unable to open: " << _path << std::endl;
        if (_cb) {
            _cb->scanner = this;
            (*_cb)();
        }
        exit();
    }
}



class DirScanner::MessageProcessEntry : public MessageBase {
public:
    MessageProcessEntry(DirScanner *self) : MessageBase(self) { }
    virtual void run() { _self->doProcessEntry(); }
};
void
DirScanner::processEntry()
{
    assertValid();
    enqueueMessage(new MessageProcessEntry(this));
}
void
DirScanner::doProcessEntry()
{
    if (sysReadDir()) {
        if ( 0 != strcmp(".", _currentEntry->d_name) &&
             0 != strcmp("..", _currentEntry->d_name)) {
            // process the entry
            std::string fullpath = _path + "/" + _currentEntry->d_name;
            struct stat buf;
            int rc = lstat(fullpath.c_str(), &buf);
            if (0 == rc) {
                if (_sg) {
                    _sg->addFile(_dirIno, _currentEntry->d_name, buf);
                }
                if (S_ISDIR(buf.st_mode)) {
                    //std::cout << "D: " << fullpath << std::endl;
                    // skip directories named ".snapshot"
                    if (_dg &&
                        0 != strcmp(".snapshot", _currentEntry->d_name)) {
                        _dg->addDirectory(fullpath);
                    }
                }
                if (S_ISLNK(buf.st_mode)) {
                    //std::cout << "L: " << fullpath << std::endl;
                    if (_sg) {
                        _sg->addLink(buf.st_ino, fullpath);
                    }
                }
            } else {
                std::cerr << "Couldn't stat: " << fullpath
                          << " " << strerror(errno) << std::endl;
            }
        }
        // do the next one
        processEntry();
    } else {
        // no more entries
        if (_cb) {
            _cb->scanner = this;
            (*_cb)();
        }
        exit();
    }
}
