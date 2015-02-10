// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#if PACKET_READER_PRIORITIZED
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <sched.h>
#endif
#include "PacketReader.h"
#include "Interface.h"
#include "Message.h"
#include "Chronicle.h"
#include "ChronicleProcess.h"
#include "NetworkHeaderParser.h"

class PacketReader::PacketReaderMsg {
	public:
		typedef enum actions {
			PR_READ,
			PR_CLOSE,
			PR_SHUTDOWN,
			PR_KILL,
			PR_STAT,
			PR_NHP_DONE
		} PacketReaderAction;
		PacketReaderMsg(PacketReaderAction action)
			: _action(action) { }
		PacketReaderAction getAction() { return _action ;}
	
	private:		
		PacketReaderAction _action;
};

class PacketReader::PacketReaderQueueCb : 
		public FDQueue<PacketReaderMsg>::FDQueueCallback {
	private:
		PacketReader *_reader;

	public:
		PacketReaderQueueCb(PacketReader *reader) 
			: _reader(reader) { }
		~PacketReaderQueueCb() { }
		void operator()(PacketReaderMsg *msg) {
			switch(msg->getAction()) {
				case PacketReaderMsg::PR_READ:
					_reader->doReadInterface();
					break;
				case PacketReaderMsg::PR_CLOSE:
					_reader->doCloseInterface();
					break;
				case PacketReaderMsg::PR_SHUTDOWN:
					_reader->doShutdownInterface();
					break;
				case PacketReaderMsg::PR_KILL:
					_reader->doKillPacketReader();
					break;
				case PacketReaderMsg::PR_STAT:
					_reader->doGetInterfaceStat();
					break;
				case PacketReaderMsg::PR_NHP_DONE:
					_reader->doGetInterfaceStat();
					break;
				default:
					;
			}
			delete msg;
		}
};

PacketReader::PacketReader(Chronicle *supervisor, Interface *interface, 
	bool basicPipeline)
	: _supervisor(supervisor), _interface(interface), _basicPipeline(basicPipeline)
{
	_queueCb = new PacketReaderQueueCb(this);
	_selfFdQueue = new FDQueue<PacketReaderMsg>(_queueCb);
	_interface->setPacketReader(this);
}

PacketReader::~PacketReader()
{
	delete _interface;
	delete _queueCb;
	delete _selfFdQueue;
}

pthread_t *
PacketReader::activate()
{
	int ret = _interface->open();
	if (ret == Interface::IF_ERR) {
		// print the error and notify the supervisor
		_interface->printError("open");
		_interface->_status = Interface::IF_ERR;
		return NULL;
	} else {
		_networkHdrParser = new NetworkHeaderParser(this, _basicPipeline,
			_supervisor);
		_interface->_status = Interface::IF_ACTIVE;
		if (pthread_create(&_thread, NULL, run, this)) {
			std::cerr << "Failed to create the PacketReader thread\n";
			return NULL;
		}
	}
	return &_thread;
}

void *
PacketReader::run(void *arg)
{
	PacketReader *pktReader = (PacketReader *) arg;
	#if PACKET_READER_PRIORITIZED
	/* // prioritizing reader threads based on their nice values
	errno = 0;
	if (setpriority(PRIO_PROCESS, 0, -20) == -1)
		perror("PacketReader::run setpriority"); */
	// prioritizing reader threads by adjusting scheduling policy & priorities
	struct sched_param schedParam;
	schedParam.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &schedParam) == -1)
		perror("PacketReader::run sched_setscheduler");
	#endif
	while (true) 
		pktReader->doReadInterface();
	return NULL;
}

void
PacketReader::handleFDQueue()
{
	_selfFdQueue->dequeue();
}

void
PacketReader::readInterface()
{
	_selfFdQueue->enqueue(new PacketReaderMsg(PacketReaderMsg::PR_READ));
}

void
PacketReader::doReadInterface() 
{
	int status = _interface->_status;
	status = _interface->read();
	switch (status) {
		case Interface::IF_ERR:
			// print the error and notify the supervisor
			_interface->printError("read");
			_interface->_status = Interface::IF_ERR;
			_supervisor->handleReaderDone(this);
			break;
		case Interface::IF_FULL:
			_interface->_status = Interface::IF_FULL;
			_supervisor->handleReaderDone(this);
			break;
		case Interface::IF_DONE:
			_supervisor->handleReaderDone(this);
			break;
		case Interface::IF_IDLE:
		case Interface::IF_ACTIVE:
		case Interface::IF_TERMINATING:
			break;
		default:
			assert(0 && "should never reach here!");
	}
}

void
PacketReader::closeInterface()
{
	_selfFdQueue->enqueue(new PacketReaderMsg(PacketReaderMsg::PR_CLOSE));
}

void
PacketReader::doCloseInterface()
{
	/* postponing all the close actions to after we make sure there are
	 * no outstanding messages from ourself or from the supervisor.		*/
	int status;
	status = _interface->_status;
	if (status == Interface::IF_ACTIVE || status == Interface::IF_IDLE)
		_interface->_status = Interface::IF_TERMINATING;
	_selfFdQueue->enqueue(new PacketReaderMsg(PacketReaderMsg::PR_SHUTDOWN));
}

void
PacketReader::doShutdownInterface()
{
	int status = _interface->_status;
	if (status == Interface::IF_TERMINATING)
		_interface->_status = Interface::IF_DONE;
	assert(_interface->_status == Interface::IF_DONE
		|| _interface->_status == Interface::IF_ERR
		|| _interface->_status == Interface::IF_FULL);
	_networkHdrParser->shutdown(this);
}

void
PacketReader::shutdownDone(Process *next)
{
	if(next == _networkHdrParser)
		_selfFdQueue->enqueue(new PacketReaderMsg(PacketReaderMsg::PR_KILL));
}

void
PacketReader::doKillPacketReader()
{
	InterfaceStat *stat = new InterfaceStat(_interface);
	_supervisor->handleReaderShutdown(this, stat);
	_interface->close();
	pthread_exit(0);
}

void 
PacketReader::processDone(Process *next)
{
	if(next == _networkHdrParser)
		_selfFdQueue->enqueue(new 
			PacketReaderMsg(PacketReaderMsg::PR_NHP_DONE));
}

void 
PacketReader::doHandleProcessDone()
{
	_interface->_status = Interface::IF_ERR;
	_supervisor->handleReaderDone(this);
}

void
PacketReader::getInterfaceStat()
{
	_selfFdQueue->enqueue(new 
		PacketReaderMsg(PacketReaderMsg::PR_STAT));
}

void
PacketReader::doGetInterfaceStat()
{
	InterfaceStat *stat = new InterfaceStat(_interface);
	_supervisor->handleReaderStat(this, stat);
}

int
PacketReader::getFDQueueFd()
{
	return _selfFdQueue->getQueueFd();
}
