// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "DirGatherer.h"
#include "DirScanner.h"
#include "ScannerFactory.h"
#include "Message.h"

class DirGatherer::MessageBase : public Message {
protected:
    DirGatherer *_process;
public:
    MessageBase(DirGatherer *process) : _process(process) { }
    virtual ~MessageBase() { }
};

DirGatherer::DirGatherer(ScannerFactory *factory) :
    _started(false), _sfactory(factory), _outstandingScanners(0),
    _maxScanners(1)
{
}

void
DirGatherer::maxScanners(unsigned scanners)
{
    _maxScanners = scanners;
}

class DirGatherer::MessageAddDirectory : public MessageBase {
    std::string _path;
public:
    MessageAddDirectory(DirGatherer *process, const std::string &path) :
        MessageBase(process), _path(path) { }
    void run() { _process->doAddDirectory(_path); }
};
void
DirGatherer::addDirectory(const std::string &path)
{
    enqueueMessage(new MessageAddDirectory(this, path));
}
void
DirGatherer::doAddDirectory(const std::string &path)
{
    _paths.push_back(path);
    startScanners();
}

class DirGatherer::MessageStart : public MessageBase {
    CompletionCb *_cb;
public:
    MessageStart(DirGatherer *process, CompletionCb *cb) :
        MessageBase(process), _cb(cb) { }
    void run() { _process->doStart(_cb); }
};
void
DirGatherer::start(CompletionCb *cb)
{
    enqueueMessage(new MessageStart(this, cb));
}
void
DirGatherer::doStart(CompletionCb *cb)
{
    _callback = cb;
    _started = true;
    startScanners();
}

class DirGatherer::ScannerFinishedCb : public DirScanner::CompletionCb {
    DirGatherer *_process;
public:
    ScannerFinishedCb(DirGatherer *process) : _process(process) { }
    void operator()() {
        _process->scannerFinished(this);
    }
};

class DirGatherer::MessageScannerFinished : public MessageBase {
public:
    MessageScannerFinished(DirGatherer *process) : MessageBase(process) { }
    void run() { _process->doScannerFinished(); }
};
void
DirGatherer::scannerFinished(ScannerFinishedCb *sf)
{
    enqueueMessage(new MessageScannerFinished(this));
    delete sf;
}
void
DirGatherer::doScannerFinished()
{
    --_outstandingScanners;
    startScanners();
    if (0 == _outstandingScanners) { // we're done scanning everything
        if (_callback) {
            (*_callback)();
        }
        exit();
    }
}

void
DirGatherer::startScanners()
{
    if (_started) {
        while (!_paths.empty() && _outstandingScanners < _maxScanners) {
            // the following actually returns a pointer to the new object,
            // but we don't care about it.
            _sfactory->newScanner(_paths.front(), new ScannerFinishedCb(this));
            ++_outstandingScanners;
            _paths.pop_front();
        }
    }
}
