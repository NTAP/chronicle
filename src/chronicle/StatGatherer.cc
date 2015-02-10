// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "StatGatherer.h"
#include "Chronicle.h"
#include "ChroniclePipeline.h"
#include "Interface.h"
#include "Message.h"
#include "RpcParser.h"
#include "NfsParser.h"
#include "PcapPduWriter.h"
#include "DsWriter.h"
#include "ChecksumModule.h"
#include "NetworkHeaderParser.h"
#include "PacketReader.h"
#include "AnalyticsModule.h"
#include "PcapPacketBufferPool.h"
#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>

#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT) 
static uint64_t cnt, avgAllocdBufsStandard, avgAllocdBufsJumbo;
#endif

class StatGatherer::MessageBase : public Message {
protected:
    StatGatherer *_self;
public:
    MessageBase(StatGatherer *self) : _self(self) { }
};

StatGatherer::StatGatherer(StatListener *statTarget, 
	AnalyticsManager *analyticsManager)	
	: Process("StatGatherer"), _statListener(statTarget), _nfsParser(0), 
	_awaitingReplies(0), _chronicle(0), _analyticsManager(analyticsManager),
	_netmapCheckNeeded(true)
{
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	_startTick = _lastTick = timestamp();
}

StatGatherer::~StatGatherer()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	printf("StatGatherer::~StatGatherer: avgAllocdBufsStandard:%lu "
		"avgAllocdBufsJumbo:%lu\n", 
		avgAllocdBufsStandard, avgAllocdBufsJumbo);
	#endif
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;	
}

class StatGatherer::MessageShutdown : public MessageBase {
protected:
    Chronicle *_src;
public:
    MessageShutdown(StatGatherer *self, Chronicle *src) :
        MessageBase(self), _src(src) { }
    void run() { _self->doShutdown(_src); }
};

void
StatGatherer::shutdown(Chronicle *src)
{
    enqueueMessage(new MessageShutdown(this, src));
}

void
StatGatherer::doShutdown(Chronicle *src)
{
    // need to make sure we don't have any outstanding requests first
    if (_awaitingReplies) {
        shutdown(src); // spin
		return ;
    }

    src->handleStatGathererShutdown();
    exit();
}


class StatGatherer::MessageTick : public MessageBase {
public:
    MessageTick(StatGatherer *self) :
        MessageBase(self) { }
    void run() { _self->doTick(); }
};

void
StatGatherer::tick()
{
    enqueueMessage(new MessageTick(this));
}

void
StatGatherer::doTick()
{
    const uint64_t INTERVAL = 10 * 1000000000llu;
    //std::cout << "... Tock ..." << std::endl;
    uint64_t curTime = timestamp();
    if (curTime > (_lastTick + INTERVAL)) {
        _lastTick = curTime;
        // go get some stats
        getInterfaceStats();
        getProcessStats();
        getMachineStats();
        getOpCounts();
		getBufPoolStats();
		getRpcParserStats();
		getAnalyticsModulesStats();
    }
}

class StatGatherer::MessageSetChronicle: public MessageBase {
protected:
    Chronicle *_c;
public:
    MessageSetChronicle(StatGatherer *self, Chronicle *c) :
        MessageBase(self), _c(c) { }
    void run() { _self->doSetChronicle(_c); }
};

void
StatGatherer::setChronicle(Chronicle *c)
{
    enqueueMessage(new MessageSetChronicle(this, c));
}

void
StatGatherer::doSetChronicle(Chronicle *c)
{
    _chronicle = c;
    doAddProcess(c);
}

void
StatGatherer::getInterfaceStats()
{
    if (_chronicle) {
        ++_awaitingReplies; // it's asynchronous
        _chronicle->getInterfaces(this);
    }
}

class StatGatherer::MessageIfList: public MessageBase {
protected:
    std::list<Interface *> _ifl;
public:
    MessageIfList(StatGatherer *self, const std::list<Interface *> &ifl) :
        MessageBase(self), _ifl(ifl) { }
    void run() { _self->doReceiveInterfaceList(_ifl); }
};

void
StatGatherer::receiveInterfaceList(const std::list<Interface *> &il)
{
    enqueueMessage(new MessageIfList(this, il));
}

void
StatGatherer::doReceiveInterfaceList(const std::list<Interface *> &il)
{
    --_awaitingReplies;

    // Retrieve packet drops directly from the NIC
    std::map<std::string, uint64_t> dropMap;
    std::ifstream procNet("/proc/net/dev");
    while (!procNet.eof() && !procNet.bad()) {
        std::string ethName;
        uint64_t dummy, drops;
        // WARNING, the following line won't work for 2.* kernels!
        procNet >> ethName >> dummy >>  dummy >>  dummy >> drops;
        //std::cout << ethName << " " << drops << std::endl;
        procNet.clear(); // unset failbit
        procNet.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (ethName.size() > 0) {
            ethName.pop_back(); // remove the trailing ':'
        }
        dropMap[ethName] = drops;
    }
    

    uint64_t curTime = timestamp();
    for (std::list<Interface *>::const_iterator i = il.begin();
         i != il.end(); ++i) {
		if (_netmapCheckNeeded && (*i)->getPacketsRead() == 0
				&& curTime > _startTick + 30*1000000000llu) {
			// detecting netmap not reading packets
			std::cerr << FONT_RED << "StatGatherer: " << (*i)->getName()
				<< " has been idle for more than 30 seconds! "
				<< "Chronicle is shutting down.\n" << FONT_DEFAULT;
			_netmapCheckNeeded = false;
			_chronicle->startShutdown();
		}
        std::string objbase("nic.");
        objbase += (*i)->getName();
        writeStats(curTime, objbase + ".nicPktsRead",
                   (*i)->getNicPacketsRead());
        writeStats(curTime, objbase + ".nicPktsDropped",
                   dropMap[(*i)->getName()]);
        writeStats(curTime, objbase + ".chroniclePktsRead",
                   (*i)->getPacketsRead());
        writeStats(curTime, objbase + ".chroniclePktsDropped",
                   (*i)->getPacketsDropped());
        writeStats(curTime, objbase + ".chronicleBytesRead",
                   (*i)->getBytesRead());
        writeStats(curTime, objbase + ".nicMaxBatch",
                   (*i)->getMaxBatchSizeRead());
        writeStats(curTime, objbase + ".nicFullBatches",
                   (*i)->getFilledBatches());
        PacketReader *r = (*i)->getPacketReader();
        if (r) {
            //writeProcessStats(curTime, r); // commented out as reader is no longer a libtask process
            NetworkHeaderParser *hp = r->_networkHdrParser;
            if (hp) {
                writeProcessStats(curTime, hp);
            }
        }
    }
	if (_netmapCheckNeeded && curTime > _startTick + 30*1000000000llu)
		_netmapCheckNeeded = false;
}

uint64_t
StatGatherer::timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000llu + ts.tv_nsec;
}

class StatGatherer::MessageAddProcess: public MessageBase {
protected:
    Process *_p;
public:
    MessageAddProcess(StatGatherer *self, Process *p) :
        MessageBase(self), _p(p) { }
    void run() { _self->doAddProcess(_p); }
};

void
StatGatherer::addProcess(Process *p)
{
    enqueueMessage(new MessageAddProcess(this, p));
}

void
StatGatherer::doAddProcess(Process *p)
{
    if (p) {
        _processList.push_back(p);
    }
}

void
StatGatherer::getProcessStats()
{
    uint64_t curTime = timestamp();
    for(std::list<Process *>::const_iterator i = _processList.begin();
        i != _processList.end(); ++i) {
        writeProcessStats(curTime, *i);
    }
}

void
StatGatherer::writeProcessStats(uint64_t timestamp, Process *p)
{
    std::string objbase("process.");
    writeStats(timestamp,
               objbase + p->processName() + "." +
			   p->getId() +
               ".backlog",
               p->pendingMessages());
}

class StatGatherer::MessageAddPipeline: public MessageBase {
protected:
    PipelineMembers *_p;
public:
    MessageAddPipeline(StatGatherer *self, PipelineMembers *p) :
        MessageBase(self), _p(p) { }
    void run() { _self->doAddPipeline(_p); }
};

void
StatGatherer::addPipeline(PipelineMembers *p)
{
    enqueueMessage(new MessageAddPipeline(this, p));
}

void
StatGatherer::doAddPipeline(PipelineMembers *p)
{
    _pipelines.push_back(p);
    doAddProcess(p->rpcParser);
	if (p->rpcParser)
		_rpcParsers.push_back(p->rpcParser);
    doAddProcess(p->nfsParser);
	if (p->nfsParser && !p->nfsParser->getId().compare("0"))
		_nfsParser = p->nfsParser;
    doAddProcess(p->checksumModule);
}

class StatGatherer::MessageAddOutputModules: public MessageBase {
	protected:
		std::list<ChronicleOutputModule *> _modules;
	public:
		MessageAddOutputModules(StatGatherer *self, 
			std::list<ChronicleOutputModule *> modules) :
			MessageBase(self), _modules(modules) { }
		void run() { _self->doAddOutputModules(_modules); }
};

void
StatGatherer::addOutputModules(
	const std::list<ChronicleOutputModule *> modules)
{
	enqueueMessage(new MessageAddOutputModules(this, modules));
}

void
StatGatherer::doAddOutputModules(
	const std::list<ChronicleOutputModule *> modules)
{
	for (std::list<ChronicleOutputModule *>::const_iterator 
			it = modules.begin();
			it != modules.end(); it++)
		doAddProcess(*it);
}

void
StatGatherer::writeStats(uint64_t nsSinceEpoch,
                         const std::string &objectPath,
                         int64_t value)
{
    if (_statListener) { // make sure we've got a target
        _statListener->writeStats(nsSinceEpoch, objectPath, value);
    }
}

void
StatGatherer::getMachineStats()
{
    getMachineCpu();
    getMachineMemory();
}

void
StatGatherer::getBufPoolStats()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	uint64_t allocatedBufsStandard, maxAllocdBufsStandard, 
		timesNoFreeBufsStandard, allocatedBufsJumbo, 
		maxAllocatedBufsJumbo, timesNoFreeBufsJumbo, now;
	_bufPool->getBufPoolStats(allocatedBufsStandard, maxAllocdBufsStandard, 
		timesNoFreeBufsStandard, allocatedBufsJumbo, maxAllocatedBufsJumbo, 
		timesNoFreeBufsJumbo);
	now = timestamp();
	cnt++;
	avgAllocdBufsStandard = 
		(avgAllocdBufsStandard * (cnt - 1) + allocatedBufsStandard)/cnt;
	avgAllocdBufsJumbo = 
		(avgAllocdBufsJumbo * (cnt - 1) + allocatedBufsJumbo)/cnt;
	std::string objbase("bufpool.");
	writeStats(now, objbase + "standard.allocated", allocatedBufsStandard);
	writeStats(now, objbase + "standard.avg", avgAllocdBufsStandard);
	writeStats(now, objbase + "standard.max", maxAllocdBufsStandard);
	writeStats(now, objbase + "standard.nobuf", timesNoFreeBufsStandard);
	writeStats(now, objbase + "jumbo.allocated", allocatedBufsJumbo);
	writeStats(now, objbase + "jumbo.avg", avgAllocdBufsJumbo);
	writeStats(now, objbase + "jumbo.max", maxAllocatedBufsJumbo);
	writeStats(now, objbase + "jumbo.nobuf", timesNoFreeBufsJumbo);
	#endif
}

void
StatGatherer::getRpcParserStats()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	unsigned long goodPduCnt, badPduCnt, 
		unmatchedCallCnt, unmatchedReplyCnt,
		completePduCallCnt, completePduReplyCnt, 
		completeHdrPduCallCnt, completeHdrPduReplyCnt,
		goodPduCntSum = 0, badPduCntSum = 0, 
		unmatchedCallCntSum = 0, unmatchedReplyCntSum = 0,
		completePduCallCntSum = 0, completePduReplyCntSum = 0,
		completeHdrPduCallCntSum = 0, completeHdrPduReplyCntSum = 0;
	uint64_t now = timestamp();
	for (std::list<RpcParser *>::const_iterator i = _rpcParsers.begin();
			i != _rpcParsers.end(); i++) {
		(*i)->getStats(goodPduCnt, badPduCnt,
			completePduCallCnt, completePduReplyCnt,
			completeHdrPduCallCnt, completeHdrPduReplyCnt,
			unmatchedCallCnt, unmatchedReplyCnt);
		goodPduCntSum += goodPduCnt;
		badPduCntSum += badPduCnt;
		completePduCallCntSum += completePduCallCnt;
		completePduReplyCntSum += completePduReplyCnt;
		completeHdrPduCallCntSum += completeHdrPduCallCnt;
		completeHdrPduReplyCntSum += completeHdrPduReplyCnt;
		unmatchedCallCntSum += unmatchedCallCnt;
		unmatchedReplyCntSum += unmatchedReplyCnt;
	}
	std::string objbase("pdu.");
	writeStats(now, objbase + "good", goodPduCntSum);
	writeStats(now, objbase + "bad", badPduCntSum);
	writeStats(now, objbase + "complete.call", completePduCallCntSum);
	writeStats(now, objbase + "complete.reply", completePduReplyCntSum);
	writeStats(now, objbase + "completehdr.call", completeHdrPduCallCntSum);
	writeStats(now, objbase + "completehdr.reply", completeHdrPduReplyCntSum);
	writeStats(now, objbase + "unmatchedcall", unmatchedCallCntSum);
	writeStats(now, objbase + "unmatchedreply", unmatchedReplyCntSum);
	#endif
}

void
StatGatherer::getAnalyticsModulesStats()
{
	 ++_awaitingReplies;
	_analyticsManager->getStats(this);
}

class StatGatherer::MessageHandleAnalyticsStats : public MessageBase {
	private:
		uint64_t _timestamp;
		std::map<std::string,int64_t> _stats;

	public:
		MessageHandleAnalyticsStats(StatGatherer *self, uint64_t timestamp,
			std::map<std::string,int64_t> stats)
			: MessageBase(self), _timestamp(timestamp), _stats(stats) { }
		void run() 
			{ _self->doHandleAnalyticsModulesStats(_timestamp, _stats); }
};

void
StatGatherer::handleAnalyticsModulesStats(uint64_t timestamp,
	const std::map<std::string,int64_t> &stats)
{
	enqueueMessage(new MessageHandleAnalyticsStats(this, timestamp, stats));
}

void
StatGatherer::doHandleAnalyticsModulesStats(uint64_t timestamp,
	std::map<std::string,int64_t> stats)
{
	--_awaitingReplies;
	std::string objbase("process.");
	for (std::map<std::string,int64_t>::iterator it = stats.begin();
			it != stats.end(); it++) {
		writeStats(timestamp, objbase + it->first, it->second);
	}
}

void
StatGatherer::getMachineMemory()
{
    std::ifstream procMeminfo("/proc/meminfo");
    std::string tag;
    uint64_t memfree = 0, buffers = 0, cached = 0, swapTotal = 0, swapFree = 0;
    do {
        procMeminfo >> tag >> memfree;
        procMeminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (procMeminfo && tag != "MemFree:");
    do {
        procMeminfo >> tag >> buffers;
        procMeminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (procMeminfo && tag != "Buffers:");
    do {
        procMeminfo >> tag >> cached;
        procMeminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (procMeminfo && tag != "Cached:");
    do {
        procMeminfo >> tag >> swapTotal;
        procMeminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (procMeminfo && tag != "SwapTotal:");
    do {
        procMeminfo >> tag >> swapFree;
        procMeminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (procMeminfo && tag != "SwapFree:");
    if (!procMeminfo) {
        // something went wrong
        return;
    }

    uint64_t now = timestamp();
    std::string memPath("system.memory.");
    writeStats(now, memPath + "free", memfree + buffers + cached);
    writeStats(now, memPath + "swap", swapTotal - swapFree);
}

void
StatGatherer::getMachineCpu()
{
    std::ifstream procStat("/proc/stat");
    // The 1st line is what we're after, and it looks like:
    // cpu  29396063 38161 62334462 41398662347 2622390 949702 179057 0 0
    std::string cpu;
    procStat >> cpu;
    if (cpu != "cpu") {
        // uh oh. We don't have the correct line for some reason
        return;
    }

    uint64_t now = timestamp();
    uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0,
        softirq = 0;
    procStat >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
    if (!procStat) {
        // something went wrong while reading the counters. We can't
        // rely on their value.
        return;
    }

    std::string statPath("system.cpu.");
    unsigned jiffyPerSec = sysconf(_SC_CLK_TCK);
    writeStats(now, statPath + "user", user/jiffyPerSec);
    writeStats(now, statPath + "nice", nice/jiffyPerSec);
    writeStats(now, statPath + "system", system/jiffyPerSec);
    writeStats(now, statPath + "idle", idle/jiffyPerSec);
    writeStats(now, statPath + "iowait", iowait/jiffyPerSec);
    writeStats(now, statPath + "irq", irq/jiffyPerSec);
    writeStats(now, statPath + "softirq", softirq/jiffyPerSec);
}

void
StatGatherer::getOpCounts()
{
    if (!_nfsParser) {
        return;
    }
	NfsParser::Nfs3OperationCounts *ops 
		= &(_nfsParser->nfs3OpCounts);
    uint64_t now = timestamp();

    std::string statPath("chronicle.capture.nfs3.");
    writeStats(now, statPath + "getattr", ops->getattr);
    writeStats(now, statPath + "setattr", ops->setattr);
    writeStats(now, statPath + "lookup", ops->lookup);
    writeStats(now, statPath + "access", ops->access);
    writeStats(now, statPath + "readlink", ops->readlink);
    writeStats(now, statPath + "read", ops->read);
    writeStats(now, statPath + "write", ops->write);
    writeStats(now, statPath + "create", ops->create);
    writeStats(now, statPath + "mkdir", ops->mkdir);
    writeStats(now, statPath + "symlink", ops->symlink);
    writeStats(now, statPath + "mknod", ops->mknod);
    writeStats(now, statPath + "remove", ops->remove);
    writeStats(now, statPath + "rmdir", ops->rmdir);
    writeStats(now, statPath + "rename", ops->rename);
    writeStats(now, statPath + "link", ops->link);
    writeStats(now, statPath + "readdir", ops->readdir);
    writeStats(now, statPath + "readdirplus", ops->readdirplus);
    writeStats(now, statPath + "fsstat", ops->fsstat);
    writeStats(now, statPath + "fsinfo", ops->fsinfo);
    writeStats(now, statPath + "pathconf", ops->pathconf);
    writeStats(now, statPath + "commit", ops->commit);
}



CarbonSocket::CarbonSocket(StatListener *next) :
    _socket(-1), _lastTry(0), _next(next)
{
    doConnect();
    if (_socket == -1) {
        std::cout << "remote statistics disconnected" << std::endl;
    }
}

CarbonSocket::~CarbonSocket()
{
    doDisconnect();
}

void
CarbonSocket::writeStats(uint64_t nsSinceEpoch,
                         const std::string &objectPath,
                         int64_t value)
{
    if (_next) {
        _next->writeStats(nsSinceEpoch, objectPath, value);
    }

    if (_socket == -1) {
        if ((nsSinceEpoch - _lastTry) > 10000000000llu) {
            _lastTry = nsSinceEpoch;
            doConnect();
        }
        if (_socket == -1) {
            return;
        }
    }

    std::ostringstream ss;
    // This is the format for Carbon (timestamp in sec)
    ss << objectPath << " " << value << " "
       << nsSinceEpoch/1000000000 << std::endl;
    const char *buf = ss.str().c_str();

    int rv = send(_socket, buf, strlen(buf), MSG_NOSIGNAL);
    if (rv < (int)strlen(buf)) {
        doDisconnect();
    }
}

void
CarbonSocket::doConnect()
{
    int rv;
    if (_socket != -1)
        doDisconnect();

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == -1)
        goto out;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    rv = connect(_socket, (sockaddr*)&addr, sizeof(addr));
    if (rv != 0)
        goto closesock;

    std::cout << "remote statistics connected" << std::endl;
    return;

closesock:
    close(_socket);
    _socket = -1;
out:
    return;
}

void
CarbonSocket::doDisconnect()
{
    if (_socket != -1) {
        close(_socket);
        _socket = -1;
        std::cout << "remote statistics disconnected" << std::endl;
    }
}
