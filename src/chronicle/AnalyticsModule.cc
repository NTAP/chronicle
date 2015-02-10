// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <stdexcept>
#include "Chronicle.h"
#include "ChronicleConfig.h"
#include "Message.h"
#include "AnalyticsModule.h"
#include "RpcParser.h"
#include "StatGatherer.h"
#include "PcapPacketBufferPool.h"

AnalyticsManager::Nfs3OperationCounts AnalyticsManager::nfs3OpCounts;

AnalyticsModule::AnalyticsModule(std::string name, AnalyticsManager *manager,
	std::string command, int id, int interval)
	: Process(name), _manager(manager), _command(command), _id(id), 
	_interval(interval)
{
	char buf[30];
	struct tm *statTm;
	std::ostringstream fileName;
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (traceDirectory[traceDirectory.size() - 1] == '/')
		traceDirectory.resize(traceDirectory.size() - 1);
	fileName << traceDirectory << "/" << processName() << "-"
		<< _id << ".txt";
	_outputFile = fileName.str();
	_output.open(_outputFile.c_str(), std::fstream::in 
		| std::fstream::out	| std::fstream::app);
	_startTime = time(0);
	statTm = localtime(&_startTime);
	strftime(buf, sizeof(buf), "%D-%T", statTm);
	std::string curTime(buf);
	_output << curTime << " "<< _command << " " << std::endl;
	_reportTime = _startTime + _interval;
}

AnalyticsModule::~AnalyticsModule()
{
	_output << std::endl;
	_output.close();
}

void
AnalyticsModule::releasePdu(PduDescriptor *pduDesc)
{
	PduDescriptor *tmpPduDesc;
	PacketDescriptor *pktDesc, *oldPktDesc;
	while (pduDesc) {
		pktDesc = pduDesc->firstPktDesc;
		do {
			oldPktDesc = pktDesc;
			pktDesc = pktDesc->next;
			_bufPool->releasePacketDescriptor(oldPktDesc);
		} while (oldPktDesc != pduDesc->lastPktDesc);
		tmpPduDesc = pduDesc;
		pduDesc = pduDesc->next;
		if (tmpPduDesc->refCount.fetch_sub(1) == 1)
			delete tmpPduDesc;
	}
}

bool
AnalyticsModule::timeToReport(struct timeval ts)
{
	if (ts.tv_sec > _reportTime) {
		_reportTime = ts.tv_sec + _interval;
		return true;
	}
	return false;
}

std::string 
AnalyticsModule::getTime(struct timeval ts)
{
	char buf[30];
	time_t statTime = ts.tv_sec;
	struct tm *statTm = localtime(&statTime);
	strftime(buf, sizeof(buf), "%D-%T", statTm);
	std::string curTime(buf);
	return curTime;
}

class AnalyticsManager::MsgBase : public Message {
	protected:
		AnalyticsManager *_manager;
		
	public:
		MsgBase(AnalyticsManager *manager) : _manager(manager) { }
		virtual ~MsgBase() { }
};

class AnalyticsManager::MsgProcessRequest : public MsgBase {
	private:
		PduDescriptor *_pduDesc;
	
	public:
		MsgProcessRequest(AnalyticsManager *manager, PduDescriptor *pduDesc) 
			: MsgBase(manager), _pduDesc(pduDesc) { }
		void run() { _manager->doProcessRequest(_pduDesc); }		
};

class AnalyticsManager::MsgProcessCommand : public MsgBase {
	private:
		std::list<std::string> _command;
	
	public:
		MsgProcessCommand(AnalyticsManager *manager, std::list<std::string> command) 
			: MsgBase(manager), _command(command) { }
		void run() { _manager->doProcessCommand(_command); }		
};

class AnalyticsManager::MsgModuleDone : public MsgBase {
	private:
		ChronicleSink *_module;
	
	public:
		MsgModuleDone(AnalyticsManager *manager, ChronicleSink *module) 
			: MsgBase(manager), _module(module) { }
		void run() { _manager->doHandleModuleDone(_module); }		
};

class AnalyticsManager::MsgShutdownModules : public MsgBase {
	public:
		MsgShutdownModules(AnalyticsManager *manager) 
			: MsgBase(manager) { }
		void run() { _manager->doShutdownModules(); }
};

class AnalyticsManager::MsgHandleModuleShutdown : public MsgBase {
	private:
		ChronicleSink *_module;

	public:
		MsgHandleModuleShutdown(AnalyticsManager *manager, ChronicleSink *module)
			: MsgBase(manager), _module(module) { }
		void run() { _manager->doHandleModuleShutdown(_module); }
};

class AnalyticsManager::MsgGetStats : public MsgBase {
	private:
		StatGatherer *_sg;

	public:
		MsgGetStats(AnalyticsManager *manager, StatGatherer *sg)
			: MsgBase(manager), _sg(sg) {}
		void run() { _manager->doHandleGetStats(_sg); }
};

AnalyticsManager::AnalyticsManager(Chronicle *supervisor, bool analyticsEnabled,
	time_t ts) 
	: Process("AnalyticsManager"), _supervisor(supervisor), 
	_enabled(analyticsEnabled), _shutdown(false), _timestamp(ts)
{
	_numModules = _killedModules = _moduleId = 0;
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		_supervisor->startShutdown();
}

AnalyticsManager::~AnalyticsManager()
{

}

void 
AnalyticsManager::processRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
	enqueueMessage(new MsgProcessRequest(this, pduDesc));
}

void
AnalyticsManager::doProcessRequest(PduDescriptor *pduDesc)
{	
	PduDescriptor *tmpPduDesc;
	PacketDescriptor *pktDesc, *oldPktDesc;
	// analytics is not enabled as part of this release
	// releasing packet and PDU descriptors
	while (pduDesc) {
		pktDesc = pduDesc->firstPktDesc;
		do {
			oldPktDesc = pktDesc;
			pktDesc = pktDesc->next;
			_bufPool->releasePacketDescriptor(oldPktDesc);
		} while (oldPktDesc != pduDesc->lastPktDesc);	
		tmpPduDesc = pduDesc;
		pduDesc = pduDesc->next;
		delete tmpPduDesc;
	}
	return ;
}

void
AnalyticsManager::updateNfsStats(PduDescriptor *pduDesc)
{
	if (pduDesc->rpcProgram != NFS_PROGRAM 
			|| pduDesc->rpcProgramVersion != NFS3_VERSION)
		return ;
	if (pduDesc->rpcMsgType == RPC_CALL) {
		switch (pduDesc->rpcProgramProcedure) {
			case NFS3PROC_NULL:
				break;
			case NFS3PROC_GETATTR:
				++nfs3OpCounts.getattr;
				break;
			case NFS3PROC_SETATTR:
				++nfs3OpCounts.setattr;
				break;
			case NFS3PROC_LOOKUP:
				++nfs3OpCounts.lookup;
				break;
			case NFS3PROC_ACCESS:
				++nfs3OpCounts.access;
				break;
			case NFS3PROC_READLINK:
				++nfs3OpCounts.readlink;
				break;
			case NFS3PROC_READ:
				++nfs3OpCounts.read;
				break;
			case NFS3PROC_WRITE:
				++nfs3OpCounts.write;
				break;
			case NFS3PROC_CREATE:
				++nfs3OpCounts.create;
				break;
			case NFS3PROC_MKDIR:
				++nfs3OpCounts.mkdir;
				break;
			case NFS3PROC_SYMLINK:
				++nfs3OpCounts.symlink;
				break;
			case NFS3PROC_MKNOD:
				++nfs3OpCounts.mknod;
				break;
			case NFS3PROC_REMOVE:
				++nfs3OpCounts.remove;
				break;
			case NFS3PROC_RMDIR:
				++nfs3OpCounts.rmdir;
				break;
			case NFS3PROC_RENAME:
				++nfs3OpCounts.rename;
				break;
			case NFS3PROC_LINK:
				++nfs3OpCounts.link;
				break;
			case NFS3PROC_READDIR:
				++nfs3OpCounts.readdir;
				break;
			case NFS3PROC_READDIRPLUS:
				++nfs3OpCounts.readdirplus;
				break;
			case NFS3PROC_FSSTAT:
				++nfs3OpCounts.fsstat;
				break;
			case NFS3PROC_FSINFO:
				++nfs3OpCounts.fsinfo;
				break;
			case NFS3PROC_PATHCONF:
				++nfs3OpCounts.pathconf;
				break;
			case NFS3PROC_COMMIT:
				++nfs3OpCounts.commit;
				break;
		}
	}
}

void 
AnalyticsManager::processCommand(std::list<std::string> command)
{
	enqueueMessage(new MsgProcessCommand(this, command));
}

void
AnalyticsManager::doProcessCommand(std::list<std::string> command)
{

}

void 
AnalyticsManager::shutdownDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgHandleModuleShutdown(this, sink));
}

void
AnalyticsManager::doHandleModuleShutdown(ChronicleSink *sink)
{
	if ((_enabled && ++_killedModules == _moduleId && _shutdown)
			|| !_enabled) {
		_supervisor->handleAnalyticsManagerShutdown();
		exit();
	} else
		_numModules--;
}

void 
AnalyticsManager::processDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgModuleDone(this, sink));
}

void
AnalyticsManager::doHandleModuleDone(ChronicleSink *sink)
{
	_supervisor->startShutdown();
}

void 
AnalyticsManager::shutdown()
{
	enqueueMessage(new MsgShutdownModules(this));
}

void
AnalyticsManager::doShutdownModules()
{
	std::map<int, AnalyticsModule *>::iterator it;
	_shutdown = true;
	if (!_enabled || !_numModules) {
		_supervisor->handleAnalyticsManagerShutdown();
		exit();
		return ;
	}
	for (it = _modules.begin(); it != _modules.end(); it++)
		it->second->shutdown(this);
}

void 
AnalyticsManager::getStats(StatGatherer *sg)
{
	enqueueMessage(new MsgGetStats(this, sg));
}

void
AnalyticsManager::doHandleGetStats(StatGatherer *sg)
{
	std::map<std::string,int64_t> processStats;
	timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
	uint64_t timestamp = ts.tv_sec * 1000000000llu + ts.tv_nsec;
	processStats.insert(std::pair<std::string, int64_t> (processName() + "."
		+ "0.backlog",	pendingMessages()));
	for (std::map<int, AnalyticsModule *>::iterator it = _modules.begin();
			it != _modules.end(); it++) {
		processStats.insert(std::pair<std::string, int64_t> (
			"AnalyticsModule." + it->second->processName() + "." 
			+ it->second->getId() + ".backlog",
			it->second->pendingMessages()));
	}
	sg->handleAnalyticsModulesStats(timestamp, processStats);
}

void
AnalyticsManager::usage()
{

}
