// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "SocketPair.h"
#include "Message.h"
#include "FDWatcher.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <iostream>

class SocketPair::ReadReadyCb : public FDWatcher::FdCallback {
    SocketPair *_sp;
public:
    ReadReadyCb(SocketPair *sp) : _sp(sp) { }
    void operator()(int fd, FDWatcher::Events event) {
        //std::cout << "read cb" << std::endl;
        _sp->receive();
    }
};

class SocketPair::WriteReadyCb : public FDWatcher::FdCallback {
    SocketPair *_sp;
public:
    WriteReadyCb(SocketPair *sp) : _sp(sp) { }
    void operator()(int fd, FDWatcher::Events event) {
        //std::cout << "write cb" << std::endl;
        _sp->send();
    }
};



SocketPair::SocketPair() :
    _srcBuf(NULL), _dstBuf(NULL), _totalBytes(0), _sentBytes(0),
    _receivedBytes(0), _rcb(new ReadReadyCb(this)),
    _wcb(new WriteReadyCb(this)), _result(NULL)
{
    _sockpair[0] = -1;
    _sockpair[1] = -1;
}

SocketPair::~SocketPair()
{
    if (_srcBuf)
        delete[] _srcBuf;
    if (_dstBuf)
        delete[] _dstBuf;
    if (-1 != _sockpair[0])
        close(_sockpair[0]);
    if (-1 != _sockpair[1])
        close(_sockpair[1]);
    delete _rcb;
    delete _wcb;
}



class SocketPair::MessageBase : public Message {
public:
    SocketPair *_self;
    MessageBase(SocketPair *self) : _self(self) { }
};

class SocketPair::MessageRun : public MessageBase {
public:
    unsigned _bytes;
    SocketPairResult *_cb;
    MessageRun(SocketPair *self, unsigned bytes, SocketPairResult *cb) :
        MessageBase(self), _bytes(bytes), _cb(cb) { };
    void run() { _self->doRun(_bytes, _cb); }
};

void
SocketPair::run(unsigned bytes, SocketPairResult *cb)
{
    enqueueMessage(new MessageRun(this, bytes, cb));
}

void
SocketPair::doRun(unsigned bytes, SocketPairResult *cb)
{
    assert(isInProcessContext());
    _totalBytes = bytes;
    _sentBytes = _receivedBytes = 0;
    _srcBuf = new char[bytes];
    memset(_srcBuf, 0, bytes);
    _dstBuf = new char[bytes];
    assert(0 == pipe2(_sockpair, O_NONBLOCK));
    _result = cb;
    timeval tv;
    gettimeofday(&tv, NULL);
    _result->elapsedSeconds = tv.tv_sec + tv.tv_usec/1000000.0;

    // start watching the descriptors
    FDWatcher::getFDWatcher()->registerFdCallback(_sockpair[0],
                                                  FDWatcher::EVENT_READ,
                                                  _rcb);
    FDWatcher::getFDWatcher()->registerFdCallback(_sockpair[1],
                                                  FDWatcher::EVENT_WRITE,
                                                  _wcb);
}



class SocketPair::MessageSend : public MessageBase {
public:
    MessageSend(SocketPair *self) : MessageBase(self) { }
    void run() { _self->doSend(); }
};

void
SocketPair::send()
{
    //std::cout << "send" << std::endl;
    enqueueMessage(new MessageSend(this));
}

void
SocketPair::doSend()
{
    assert(isInProcessContext());
    //std::cout << "doSend" << std::endl;
    unsigned bytesRemaining = _totalBytes - _sentBytes;
    char *bufp = &_srcBuf[_sentBytes];
    ssize_t written = write(_sockpair[1], bufp, bytesRemaining);
    ++_result->sendCalls;
    if (written > 0) {
        _sentBytes += written;
    } else if (written == 0) {
        std::cout << "write couldn't make progress" << std::endl;
    } else {
        perror("writing to pipe");
        assert(0);
    }
    if (_sentBytes < _totalBytes) {
        // more to write; re-register
        FDWatcher::getFDWatcher()->registerFdCallback(_sockpair[1],
                                                      FDWatcher::EVENT_WRITE,
                                                      _wcb);
    } else {
        // we're done, close it.
        close(_sockpair[1]);
        _sockpair[1] = -1;
    }
}



class SocketPair::MessageReceive : public MessageBase {
public:
    MessageReceive(SocketPair *self) : MessageBase(self) { }
    void run() { _self->doReceive(); }
};

void
SocketPair::receive()
{
    enqueueMessage(new MessageReceive(this));
}

void
SocketPair::doReceive()
{
    assert(isInProcessContext());
    unsigned bytesRemaining = _totalBytes - _receivedBytes;
    char *bufp = &_dstBuf[_receivedBytes];
    ssize_t bytes = read(_sockpair[0], bufp, bytesRemaining);
    ++_result->receiveCalls;
    if (bytes > 0) {
        _receivedBytes += bytes;
    } else if (bytes == 0) {
        std::cout << "read couldn't make progress" << std::endl;
    } else {
        perror("reading from pipe");
        assert(0);
    }
    if (_receivedBytes < _totalBytes) {
        // more to read; re-register
        FDWatcher::getFDWatcher()->registerFdCallback(_sockpair[0],
                                                      FDWatcher::EVENT_READ,
                                                      _rcb);
    } else {
        close(_sockpair[0]);
        _sockpair[0] = -1;
        // we're done. return the result
        _result->bytesTransferred = _receivedBytes;
        timeval tv;
        gettimeofday(&tv, NULL);
        double now = tv.tv_sec + tv.tv_usec/1000000.0;
        _result->elapsedSeconds = now - _result->elapsedSeconds;
        (*_result)();
        exit();
    }
}
