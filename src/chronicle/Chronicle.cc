// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <cstdlib>
#include "Chronicle.h"
#include "PcapPacketBufferPool.h"
#include "ChroniclePipeline.h"
#include "OutputModule.h"
#include "AnalyticsModule.h"
#include "PacketReaderFactory.h"
#include "PacketReader.h"
#include "Interface.h"
#include "PcapInterface.h"
#include "NetmapInterface.h"
#include "Message.h"
#include "FDWatcher.h"
#include "CommandChannel.h"
#include "ChroniclePipeline.h"
#include "StatGatherer.h"

void _exit(int status)
{
	exit(status);
}

class Chronicle::MsgBase : public Message {
	protected:
		Chronicle *_supervisor;

	public:
		MsgBase(Chronicle *supervisor) : _supervisor(supervisor) { }
		virtual ~MsgBase() { }
};

class Chronicle::MsgStartShutdown : public MsgBase {
	public:
		MsgStartShutdown(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() { _supervisor->doStartShutdown(); }
};

class Chronicle::MsgAddInterface : public MsgBase {
	private:
		Interface *_interface;
		
	public:
		MsgAddInterface(Chronicle *supervisor, Interface *interface)
			: MsgBase(supervisor), _interface(interface) { }
		void run() { _supervisor->doAddInterface(_interface); }
};

class Chronicle::MsgStatGathererShutdown : public MsgBase {
	public:
		MsgStatGathererShutdown(Chronicle *supervisor) 
			: MsgBase(supervisor) { }
		void run() { _supervisor->doHandleStatGathererShutdown(); }
};

class Chronicle::MsgHandleFDWatcher : public MsgBase {
	public:
		MsgHandleFDWatcher(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() { _supervisor->doHandleFdWatcher(); }
};

class Chronicle::MsgPipelineManagerShutdown : public MsgBase {
	public:
		MsgPipelineManagerShutdown(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() { _supervisor->doHandlePipelineManagerShutdown(); }
};

class Chronicle::MsgOutputManagerShutdown : public MsgBase {
	public:
		MsgOutputManagerShutdown(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() { _supervisor->doHandleOutputManagerShutdown(); }
};

class Chronicle::MsgAnalyticsManagerShutdown : public MsgBase {
	public:
		MsgAnalyticsManagerShutdown(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() { _supervisor->doHandleAnalyticsManagerShutdown(); }
};

class Chronicle::MsgCommandChannel : public MsgBase {
	private: 
		int _command;
		std::list<std::string> _args;
	
	public:
		MsgCommandChannel(Chronicle *supervisor, int command, 
			std::list<std::string> args) : MsgBase(supervisor), 
			_command(command), _args(args) { }
		void run() 
			{ _supervisor->doHandleCommandChannel(_command, _args); }
};

class Chronicle::MsgDeadChannel : public MsgBase {
	public:
		MsgDeadChannel(Chronicle *supervisor) : MsgBase(supervisor) { }
		void run() 
			{ _supervisor->doHandleDeadChannel(); }
};

class Chronicle::MsgGetInterfaces : public MsgBase {
    InterfaceListReceiver *_ilr;
public:
    MsgGetInterfaces(Chronicle *supervisor, InterfaceListReceiver *ilr) :
        MsgBase(supervisor), _ilr(ilr) { }
    void run() { _supervisor->doGetInterfaces(_ilr); }
};

class Chronicle::ChronicleQueueMsg {
	public:
		typedef enum actions {
			CHRON_READER_DONE,
			CHRON_READER_SHUTDOWN,
			CHRON_READER_STAT
		} ChronicleAction;
		ChronicleQueueMsg(ChronicleAction action, PacketReader *reader,
				InterfaceStat *stat = NULL) 
			: _action(action), _reader(reader), _stat(stat) { }
		
		ChronicleAction _action;
		PacketReader *_reader;
		InterfaceStat *_stat;
};

class Chronicle::ChronicleQueueCb : public 
		FDQueue<ChronicleQueueMsg>::FDQueueCallback {
	public:
		ChronicleQueueCb(Chronicle *supervisor) : _supervisor(supervisor) { }
		~ChronicleQueueCb() { }
		void operator()(ChronicleQueueMsg *msg) {
			switch(msg->_action) {
				case ChronicleQueueMsg::CHRON_READER_DONE:
					_supervisor->doShutdownReader(msg->_reader);
					break;
				case ChronicleQueueMsg::CHRON_READER_SHUTDOWN:
					_supervisor->doHandleReaderShutdown(msg->_reader, 
						msg->_stat);
					break;
				case ChronicleQueueMsg::CHRON_READER_STAT:
					_supervisor->doHandleReaderStat(msg->_reader, 
						msg->_stat);
					break;
				default:
					;
			}
			delete msg;
		}

	private:
		Chronicle *_supervisor;
};

class Chronicle::FDWatcherCb : public FDWatcher::FdCallback {
	private:
		Chronicle *_supervisor;

	public:
		FDWatcherCb(Chronicle *supervisor) : _supervisor(supervisor) { } 
		~FDWatcherCb() { }
		void operator() (int fd, FDWatcher::Events event) {
			_supervisor->handleFdWatcher();			
		}
};

Chronicle::Chronicle(uint32_t numInterfaces, int channelFd, 
	uint8_t pipelineType, uint32_t numPipelines, uint8_t outputFormat,
	uint32_t numOutputManagers, int snapLength, bool analyticsEnabled,
	CompletionCb *compCb, InterfaceStatCb *statCb) :
		Process("Chronicle"), _numInterfaces(numInterfaces),
		_pipelineType(pipelineType), _compCb(compCb), _statCb(statCb)
{
	_shutdownStarted = false;
	PacketBufferPool *_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		_exit(EXIT_FAILURE);
	_channel = new CommandChannel(this, channelFd, FDWatcher::EVENT_READ);
	time_t timestamp = time(0);
	_analyticsManager = new AnalyticsManager(this, analyticsEnabled, timestamp);
	_outputManager = new OutputManager(this, outputFormat, numOutputManagers,
		snapLength, _analyticsManager, timestamp);
	_pipelineManager = new PipelineManager(this, _pipelineType, numPipelines,
		_outputManager, snapLength);
    StatListener *listener = _outputManager->getStatListener(); 
	_carbon = new CarbonSocket(listener);
    _statModule = new StatGatherer(_carbon, _analyticsManager);
    _statModule->setChronicle(this);
	_pipelineManager->addPipelineMembers(_statModule);
	_outputManager->addOutputModules(_statModule);
	_queueCb = new ChronicleQueueCb(this);
	_selfFdQueue = new FDQueue<ChronicleQueueMsg> (_queueCb);
	_queueFd = _selfFdQueue->getQueueFd();
	_fdWatcherCb = new FDWatcherCb(this);
	FDWatcher::getFDWatcher()->registerFdCallback(
		_queueFd, FDWatcher::EVENT_READ, _fdWatcherCb);
}

Chronicle::~Chronicle()
{
	std::list<InterfaceStat *>::iterator it;
	for (it = doneInterfaceStats.begin(); it != doneInterfaceStats.end(); it++) {
		delete *it;
	}
    if (_carbon)
        delete _carbon;
	delete _queueCb;	
	delete _selfFdQueue;
	delete _fdWatcherCb;
}

void
Chronicle::startShutdown()
{
	enqueueMessage(new MsgStartShutdown(this));
}

void
Chronicle::doStartShutdown()
{
	if (!_shutdownStarted) {
		_shutdownStarted = true;
		doCloseReaders();
	}
}

void
Chronicle::addInterface(Interface *i)
{
	enqueueMessage(new MsgAddInterface(this, i));	
}

void
Chronicle::doAddInterface(Interface *interface)
{
	if (_shutdownStarted)
		return;
	if ((interface->_type == "PcapOnlineInterface" || 
			interface->_type == "NetmapInterface")
			&& !strcmp(interface->getName(), "DEFAULT")) {				
		// adding the default interface 
		char *defInterface;
		defInterface = PcapOnlineInterface::getDefaultNic();
		if (defInterface != NULL) {
			Interface *i;
			if (interface->_type == "PcapOnlineInterface") {
				i = new PcapOnlineInterface(defInterface, 
					interface->getBatchSize(), interface->getFilter(), 
					interface->getToCopy());
			}
			else {
				i = new NetmapInterface(defInterface, interface->getBatchSize(), 
					interface->getToCopy());
			}
			doAddInterface(i);
		}
		delete interface;
	} else if ((interface->_type == "PcapOnlineInterface" ||
			interface->_type == "NetmapInterface")
			&& !strcmp(interface->getName(), "ALL")) {					
		// adding all the network interfaces 
		std::list<char *> nics;
		std::list<char *>::const_iterator it;
		if (PcapOnlineInterface::getAllNics(nics) != -1) {		
			_numInterfaces += (nics.size() - 1);
			for (it = nics.begin(); it != nics.end(); it++) {
				Interface *i;
				if (interface->_type == "PcapOnlineInterface") {
					i = new PcapOnlineInterface(*it, interface->getBatchSize(), 
						interface->getFilter(), 
						interface->getToCopy());
				} else {
					i = new NetmapInterface(*it, interface->getBatchSize(), 
						interface->getToCopy());
				}
				doAddInterface(i);
			}
		}
		if (nics.size() == 0)
			std::cerr << FONT_RED << "root privilege is required "
				<< "to open pcap online interfaces!\n" << FONT_DEFAULT;
		delete interface;
		if (_numInterfaces == 0) 
			_channel->closeChannel();
	}
	else {
		PacketReader *reader;
		PacketReaderFactory factory;
		reader = factory.newPacketReader(this, interface, 
			(_pipelineType == PCAP_PIPELINE));
		if (reader->activate() == NULL) {
			if (--_numInterfaces == 0) {
				delete reader;
				startShutdown();
			}
		} else 
			_readers.push_back(reader);
	}
}

void
Chronicle::doGetReadersStat()
{
	// getting the stats from all the live interfaces
	std::list<PacketReader *>::const_iterator it1;
	for (it1 = _readers.begin(); it1 != _readers.end(); it1++) 
		(*it1)->getInterfaceStat();

	// getting the stats from all the done interfaces
	std::list<InterfaceStat *>::const_iterator it2;
	for (it2 = doneInterfaceStats.begin(); it2 != doneInterfaceStats.end(); it2++) {
		(*_statCb)(*it2);
	}
}

void
Chronicle::handleReaderStat(PacketReader *reader, InterfaceStat *stat)
{
	_selfFdQueue->enqueue(
		new ChronicleQueueMsg(ChronicleQueueMsg::CHRON_READER_STAT,
			reader, stat));
}

void
Chronicle::doHandleReaderStat(PacketReader *reader, InterfaceStat *stat)
{
	(*_statCb)(stat);
	delete stat;
}

void
Chronicle::handleReaderDone(PacketReader *reader)
{
	_selfFdQueue->enqueue(
		new ChronicleQueueMsg(ChronicleQueueMsg::CHRON_READER_DONE,	reader));
}

void
Chronicle::doShutdownReader(PacketReader *reader)
{
	if (reader->getInterface()->getStatus() == Interface::IF_FULL) {
		doStartShutdown();
		return ;
	}
	std::list<PacketReader *>::iterator it;
	for (it = _readers.begin(); it != _readers.end(); it++) {
		if (*it == reader) {
			std::cout << FONT_BLUE << "[" << reader->getInterface()->getName()
				<< "] closed" << FONT_DEFAULT << std::endl;
			_suspendedReaders.push_back(reader);
			_readers.erase(it);
			reader->closeInterface();
			break;
		}
	}
}

void
Chronicle::doCloseReaders()
{
	std::list<PacketReader *>::iterator it;
	PacketReader *reader;
	if (_readers.size() == 0 && _suspendedReaders.size() == 0) {
		// in case the readers never got activated
		doShutdownStatGatherer();
	}
	for (it = _readers.begin(); it != _readers.end();) {
		reader = *it;
		_suspendedReaders.push_back(reader);
		it = _readers.erase(it);
		std::cout << FONT_BLUE << "[" << reader->getInterface()->getName() 
			<< "] closed" << FONT_DEFAULT 
			<< std::endl;
		reader->closeInterface();
	}
}

void
Chronicle::handleReaderShutdown(PacketReader *reader, InterfaceStat *stat)
{
	_selfFdQueue->enqueue(
		new ChronicleQueueMsg(ChronicleQueueMsg::CHRON_READER_SHUTDOWN,	
			reader, stat));
}

void 
Chronicle::doHandleReaderShutdown(PacketReader *reader, InterfaceStat *stat)
{
	std::list<PacketReader *>::iterator it;
	for (it = _suspendedReaders.begin(); it != _suspendedReaders.end(); it++) {
		if (*it == reader) {
			doneInterfaceStats.push_back(stat); 
			if (reader->deactivate() == 0) {
				_suspendedReaders.erase(it);
				_doneReaders.push_back(reader);
			} else
				std::cerr << "Chronicle::doHandleReaderShutdown: "
					"reader->join() failed!\n";
			break;
		}
	}
	if (_readers.size() == 0 && _suspendedReaders.size() == 0) {
		_shutdownStarted = true; // in case readers are individually closed
		doShutdownStatGatherer();
	}
}

StatGatherer *
Chronicle::getStatGatherer()
{
    return _statModule;
}

void
Chronicle::doShutdownStatGatherer()
{
    _statModule->shutdown(this);
}

void
Chronicle::handleStatGathererShutdown()
{
	enqueueMessage(new MsgStatGathererShutdown(this));
}

void
Chronicle::doHandleStatGathererShutdown()
{
    _statModule = 0;
	_pipelineManager->shutdown();
}

void 
Chronicle::handlePipelineManagerShutdown()
{
	enqueueMessage(new MsgPipelineManagerShutdown(this));
}

void 
Chronicle::doHandlePipelineManagerShutdown()
{
	_outputManager->shutdown();	
}

void
Chronicle::handleOutputManagerShutdown()
{
	enqueueMessage(new MsgOutputManagerShutdown(this));
}

void
Chronicle::doHandleOutputManagerShutdown()
{
	_analyticsManager->shutdown();
}

void
Chronicle::handleAnalyticsManagerShutdown()
{
	enqueueMessage(new MsgAnalyticsManagerShutdown(this));
}

void
Chronicle::doHandleAnalyticsManagerShutdown()
{
	_channel->closeChannel();
}

void
Chronicle::handleCommandChannel(int command, std::list<std::string> args)
{
	enqueueMessage(new MsgCommandChannel(this, command, args));
}

void
Chronicle::doHandleCommandChannel(int command, std::list<std::string> args)
{
	std::string interface = "";
	switch(command) {
		case CommandChannel::OP_INTERFACE_CLOSE:
			interface = args.front();
			if (!interface.compare("ALL"))
				startShutdown();
			else {
				// find the interface with the given name and close it
				PacketReader *r = findLiveReader(interface.c_str());
				if (r)
					doShutdownReader(r);
				else
					std::cerr << FONT_RED 
						<< "Chronicle::doHandleCommandChannel: "
						<< "Invalid interface!\n" << FONT_DEFAULT;
			}
			break;
		case CommandChannel::OP_INTERFACE_STAT:
			interface = args.front();
			if (!interface.compare("ALL")) {
				doGetReadersStat();
			} else {
				// find the reader with the given interface and gets its stat
				PacketReader *r = findLiveReader(interface.c_str());
				if (r)
					r->getInterfaceStat();
				else { // if not alive, find it among done interfaces
					InterfaceStat *s = findDoneInterface(interface.c_str());
					if (s) 	
						(*_statCb)(s);
					else
						std::cerr << FONT_RED 
							<< "Chronicle::doHandleCommandChannel: "
							<< "Invalid interface!\n" << FONT_DEFAULT;
				}
			}
			break;
		case CommandChannel::OP_ANALYTICS:
			_analyticsManager->processCommand(args);	
			break;
		case CommandChannel::OP_BAD_CHANNEL:
		case CommandChannel::OP_CLOSED_CHANNEL:
			std::cerr << FONT_RED << "Chronicle::doHandleCommandChannel: "
				"CommandChannel out of operation!\n" << FONT_DEFAULT;
		case CommandChannel::OP_QUIT:
			startShutdown();
			break;			
		default:
			;		
	}
}

void
Chronicle::handleDeadChannel()
{
	enqueueMessage(new MsgDeadChannel(this));
}

void
Chronicle::doHandleDeadChannel()
{
	_channel->killChannel();
	if (_doneReaders.size()) {
		do {
			PacketReader *reader;
			std::list<PacketReader *>::iterator it;
			it = _doneReaders.begin();
			reader = *it;
			_doneReaders.erase(it);
			delete reader;		
		} while(!_doneReaders.empty());
	}
	std::cout << "Chronicle quiting and printing stats!\n";
	(*_compCb) (this, _statCb);
	exit();
}

PacketReader *
Chronicle::findLiveReader(const char *interfaceName)
{
	std::list<PacketReader *>::const_iterator it;
	for (it = _readers.begin(); it != _readers.end(); it++) {
		if (!strcmp(interfaceName, (*it)->getInterface()->getName()))
			return *it;
	}
	return NULL;
}

InterfaceStat *
Chronicle::findDoneInterface(const char *interfaceName)
{
	std::list<InterfaceStat *>::const_iterator it;
	for (it = doneInterfaceStats.begin(); it != doneInterfaceStats.end(); it++) {
		if (!strcmp(interfaceName, (*it)->getName()))
			return *it;
	}
	return NULL;
}

void
Chronicle::getInterfaces(InterfaceListReceiver *ilr)
{
	enqueueMessage(new MsgGetInterfaces(this, ilr));
}

void
Chronicle::doGetInterfaces(InterfaceListReceiver *ilr)
{
    std::list<Interface *> ifList;
    for (std::list<PacketReader *>::const_iterator i = _readers.begin();
         i != _readers.end(); ++i) {
        ifList.push_back((*i)->getInterface());
    }
    for (std::list<PacketReader *>::const_iterator i = _doneReaders.begin();
         i != _doneReaders.end(); ++i) {
        ifList.push_back((*i)->getInterface());
    }
    ilr->receiveInterfaceList(ifList);
}

void
Chronicle::handleFdWatcher()
{
	enqueueMessage(new MsgHandleFDWatcher(this));
}

void
Chronicle::doHandleFdWatcher()
{
	FDWatcher::getFDWatcher()->registerFdCallback(
		_queueFd, FDWatcher::EVENT_READ, _fdWatcherCb); 
	_selfFdQueue->dequeue();
}

