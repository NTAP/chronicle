// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include <string>
#include <iostream>
#include "ChroniclePipeline.h"
#include "Chronicle.h"
#include "ChronicleProcess.h"
#include "ChronicleConfig.h"
#include "OutputModule.h"
#include "AnalyticsModule.h"
#include "Message.h"
#include "RpcParserFactory.h"
#include "PcapWriterFactory.h"
#include "NfsParser.h"
#include "ChecksumModule.h"
#include "StatGatherer.h"

ChroniclePipeline *PipelineManager::_pipelines[MAX_PIPELINE_NUM];
extern uint16_t hash16BitBobJenkins(uint32_t srcIP, uint32_t destIP, 
	uint16_t srcPort, uint16_t destPort, uint8_t protocol);

class PipelineManager::MsgBase : public Message {
	protected:
		PipelineManager *_manager;
	public:
		MsgBase(PipelineManager *manager) : _manager(manager) { }
		virtual ~MsgBase() { }
};

class PipelineManager::MsgPipelineDone : public MsgBase {
	private:
		ChronicleSink *_sink;
	public:
		MsgPipelineDone(PipelineManager *manager, ChronicleSink *sink) 
			: MsgBase(manager), _sink(sink) { }
		void run() { _manager->doHandlePipelineDone(_sink); }
};

class PipelineManager::MsgShutdownPipelines : public MsgBase {
	public:
		MsgShutdownPipelines(PipelineManager *manager) 
			: MsgBase(manager) { }
		void run() { _manager->doShutdownPipelines(); }
};

class PipelineManager::MsgHandlePipelineShutdown : public MsgBase {
	private:
		ChronicleSink *_sink;
	public:
		MsgHandlePipelineShutdown(PipelineManager *manager, ChronicleSink *sink) 
			: MsgBase(manager), _sink(sink) { }
		void run() { _manager->doHandlePipelineShutdown(_sink); }
};

PipelineManager::PipelineManager(Chronicle *supervisor, uint8_t pipelineType,
		uint32_t num, OutputManager *outputModule, int snapLen)
	: Process("PipelineManager"), _supervisor(supervisor), _numPipelines(num)
{
	_killedPipelines = 0;
	switch(pipelineType) {
		case NFS_PIPELINE:
			for (uint32_t i = 0; i < MAX_PIPELINE_NUM; i++) {
				if (i < _numPipelines)
					_pipelines[i] = new NfsPipeline(this, i, outputModule);
				else
					_pipelines[i] = _pipelines[i % _numPipelines];
			}
			break;
		case PCAP_PIPELINE:
			for (uint32_t i = 0; i < MAX_PIPELINE_NUM; i++) {
				if (i < _numPipelines)
					_pipelines[i] = new PcapPipeline(this, i, snapLen);
				else
					_pipelines[i] = _pipelines[i % _numPipelines];
			}
			break;
	}
}

void
PipelineManager::processDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgPipelineDone(this, sink));
}

void
PipelineManager::doHandlePipelineDone(ChronicleSink *sink)
{
	// If one pipeline is done as a result of some problem (e.g., inadequate
	// buffer space, insufficient space to write traces, etc.), then other 
	// pipelines will soon encounter the same problem and need to be brought
	// down. We first notify the supervisor to start the shutdown process.
	_supervisor->startShutdown();
}

void
PipelineManager::shutdown()
{
	enqueueMessage(new MsgShutdownPipelines(this));
}

void
PipelineManager::doShutdownPipelines()
{
	for (uint32_t i = 0; i < _numPipelines; i++)
		_pipelines[i]->shutdown();
}

void
PipelineManager::shutdownDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgHandlePipelineShutdown(this, sink));
}

void
PipelineManager::doHandlePipelineShutdown(ChronicleSink *sink)
{
	uint32_t i;
	PacketDescReceiver *head = static_cast<PacketDescReceiver *> (sink);
	for (i = 0; i < _numPipelines; i++) {
		if (head == _sinks[i]) {
			std::cout << FONT_BLUE
				<< "PipelineManager: "
				<< "pipeline " << i << " terminated\n"
				<< FONT_DEFAULT;
			break;
		}
	}
	if (i == _numPipelines) {
		std::cout << FONT_BLUE
			<< "PipelineManager: "
			<< "unrecognized pipeline!\n"
			<< FONT_DEFAULT;
		return ;
	}
	if (++_killedPipelines == _numPipelines) {
		_supervisor->handlePipelineManagerShutdown();
		exit();
	}
}

void
PipelineManager::addPipelineMembers(StatGatherer *statGatherer)
{
	for (uint32_t i = 0; i < _numPipelines; i++)
		statGatherer->addPipeline(_pipelines[i]->getPipelineMembers());
}

ChroniclePipeline *
PipelineManager::findPipeline(uint32_t srcIP, uint32_t destIP, 
	uint16_t srcPort, uint16_t destPort, uint8_t protocol)
{
	uint32_t pipelineId;
	// making sure packets in either direction of a flow end up in the same
	// pipeline.
	if (srcIP > destIP)
		pipelineId = hash16BitBobJenkins(srcIP, destIP, srcPort, destPort, 
			protocol) & (MAX_PIPELINE_NUM - 1);
	else
		pipelineId = hash16BitBobJenkins(destIP, srcIP, destPort, srcPort,
			protocol) & (MAX_PIPELINE_NUM - 1);
	return _pipelines[pipelineId];
}

ChroniclePipeline::~ChroniclePipeline()
{

}

NfsPipeline::NfsPipeline(PipelineManager *manager, uint32_t id, 
		OutputManager *outputModule)
	: ChroniclePipeline(manager, id)
{
	RpcParserFactory f;
	RpcParser *rpcParser = f.newRpcParser(_manager, id);
    _pipelineMembers.rpcParser = rpcParser;
	_head = static_cast<PacketDescReceiver *> (rpcParser);
	manager->addSink(id, _head);
	NfsParser *nfsParser = new NfsParser(rpcParser, id, outputModule);
	rpcParser->setSink(nfsParser);
    _pipelineMembers.nfsParser = nfsParser;

    ChecksumModule *xsum = 0;
    if (dsEnableIpChecksum) {
        xsum = new ChecksumModule(nfsParser, id, outputModule);
        _pipelineMembers.checksumModule = xsum;
        nfsParser->setSink(xsum);
    }
}

PcapPipeline::PcapPipeline(PipelineManager *manager, uint32_t id,
	int snapLen)
	: ChroniclePipeline(manager, id)
{
	PcapWriterFactory f;
	_head = static_cast<PacketDescReceiver *> 
		(f.newPcapWriter(_manager, id, snapLen));
	manager->addSink(id, _head);
}

