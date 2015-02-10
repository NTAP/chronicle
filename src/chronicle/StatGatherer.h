// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef STATGATHERER_H
#define STATGATHERER_H

#include <list>
#include <map>
#include "Chronicle.h"
#include "ChronicleProcess.h"
#include "Process.h"

class PipelineMembers;
class ChronicleOutputModule;
class PacketBufferPool;
class RpcParser;
class NfsParser;
class AnalyticsManager;

class StatListener {
public:
    virtual ~StatListener() { }
    virtual void writeStats(uint64_t nsSinceEpoch,
                            const std::string &objectPath,
                            int64_t value) = 0;
};

/**
 * This is a StatListener that provides a data feed to a socket so
 * that it can be imported into Carbon. The class also can relay the
 * data feed to another StatListener, allowing this one to be
 * interposed in front on an existing listener. It does not take
 * ownership of the next listener.
 */
class CarbonSocket : public StatListener {
public:
    CarbonSocket(StatListener *next = 0);
    ~CarbonSocket();
    virtual void writeStats(uint64_t nsSinceEpoch,
                            const std::string &objectPath,
                            int64_t value);

private:
    /// The port that Carbon listens on
    static const uint16_t PORT = 2003;

    int _socket;
    uint64_t _lastTry;
    StatListener *_next;

    void doConnect();
    void doDisconnect();
};

class StatGatherer : public Process,
                     public Chronicle::InterfaceListReceiver {
public:
    StatGatherer(StatListener *statTarget, AnalyticsManager *analyticsManager);
    ~StatGatherer();

    void setChronicle(Chronicle *c); // for interface stats
    virtual void receiveInterfaceList(const std::list<Interface *> &il);

    void addProcess(Process *p); // to monitor a process
    void addPipeline(PipelineMembers *p); // to monitor a pipeline
	void addOutputModules(const std::list<ChronicleOutputModule *>); // to monitor output modules
	void handleAnalyticsModulesStats(uint64_t timestamp,
		const std::map<std::string,int64_t> &stats); // to monitor analytics modules stats

    virtual void shutdown(Chronicle *src);

    // Notify the StatGatherer that time has advanced and it should
    // record the current system statistics
    void tick();

private:
    class MessageBase;
    class MessageTick;
    class MessageShutdown;
    class MessageIfList;
    class MessageSetChronicle;
    class MessageAddProcess;
    class MessageAddPipeline;
	class MessageAddOutputModules;
	class MessageHandleAnalyticsStats;

    void doShutdown(Chronicle *src);
    void doTick();

    void doSetChronicle(Chronicle *c);
    void getInterfaceStats();
    void doReceiveInterfaceList(const std::list<Interface *> &il);
	void doHandleAnalyticsModulesStats(uint64_t timestamp,
		std::map<std::string,int64_t> stats);

    void doAddProcess(Process *p);
    void getProcessStats();
    void writeProcessStats(uint64_t timestamp, Process *p);

    void getMachineStats();
	void getBufPoolStats();
	void getRpcParserStats();
	void getAnalyticsModulesStats();
    void getMachineCpu();
    void getMachineMemory();

    void getOpCounts();

    void doAddPipeline(PipelineMembers *p);
	void doAddOutputModules(const std::list<ChronicleOutputModule *> modules);

    uint64_t timestamp();
    void writeStats(uint64_t nsSinceEpoch,
                    const std::string &objectPath,
                    int64_t value);

    void connectSocket();

    StatListener *_statListener;
	NfsParser *_nfsParser;
	uint64_t _startTick;
    uint64_t _lastTick;
    unsigned _awaitingReplies;

    Chronicle *_chronicle;
	AnalyticsManager *_analyticsManager;
	PacketBufferPool *_bufPool;
    std::list<Process *> _processList;
    std::list<PipelineMembers *> _pipelines;
	std::list<RpcParser *> _rpcParsers;
	bool _netmapCheckNeeded;
};

#endif // STATGATHERER_H
