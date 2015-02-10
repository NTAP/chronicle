// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLE_PIPELINE_H
#define CHRONICLE_PIPELINE_H

#include "ChronicleProcess.h"
#include "Process.h"

class Chronicle;
class ChroniclePipeline;
class OutputManager;
class StatGatherer;
class RpcParser;
class NfsParser;
class ChecksumModule;

struct PipelineMembers {
    RpcParser *rpcParser;
    NfsParser *nfsParser;
    ChecksumModule *checksumModule;
    PipelineMembers() : rpcParser(0), nfsParser(0), checksumModule(0) { }
};

/**
 * A class that manages different Chronicle Pipelines
 */
class PipelineManager : public Process, public ChronicleSource {
	public:
		PipelineManager(Chronicle *supervisor, uint8_t pipelineType,
			uint32_t num, OutputManager *outputModule, int snapLength);
		~PipelineManager() { } 
		/**
		 * invoked when the pipeline has completely shut down
		 * @param[in] sink The process that has shut down
		 */
		void shutdownDone(ChronicleSink *sink);
		/**
		 * invoked when a pipeline has encountered an error and should be 
		 * shutdown
		 * @param[in] sink The process that is done
		 */
		void processDone(ChronicleSink *sink);
		/**
		 * invoked by the supervisor to shut down the pipelines
		 */
		void shutdown();
		/**
		 * adds a pipeline's head process to the sink table
		 */
		void addSink(uint32_t id, PacketDescReceiver *head) { _sinks[id] = head; }
		/**
		 * adds pipeline members to the StatGatherer module
		 */
		void addPipelineMembers(StatGatherer *statGatherer);
		/** 
		 *
		 */
		static ChroniclePipeline* findPipeline(uint32_t srcIP, uint32_t destIP, 
			uint16_t srcPort, uint16_t destPort, uint8_t protocol);

	private:
		class MsgBase;
		class MsgPipelineDone;
		class MsgShutdownPipelines;
		class MsgHandlePipelineShutdown;

		void doHandlePipelineDone(ChronicleSink *sink);
		void doShutdownPipelines();
		void doHandlePipelineShutdown(ChronicleSink *sink);

		// pipeline table
		static ChroniclePipeline *_pipelines[MAX_PIPELINE_NUM];
		// table for all the head process in the pipeline
		PacketDescReceiver *_sinks[MAX_PIPELINE_NUM];
		// supervisor
		Chronicle *_supervisor;
		// number of pipelines
		uint32_t _numPipelines;
		// number of killed pipelines
		uint32_t _killedPipelines;
};

/**
 * An abstract parent class for different types of Chronicle pipelines.
 * This class also implements the pipeline table.
 */
class ChroniclePipeline {
	public:
		ChroniclePipeline(PipelineManager *manager, unsigned id) 
			: _manager(manager), _id(id) { }
		virtual ~ChroniclePipeline() = 0;
		/**
		 * retrieves the head process in the pipeline
		 * @returns the head process in the pipeline
		 */
		PacketDescReceiver *getPipelineHeadProcess() { return _head; }
		/**
		 * retrives the modules belonging to this pipeline
		 * @returns the pipeline members
		 */
		PipelineMembers *getPipelineMembers() { return &_pipelineMembers; }
		void shutdown() { _head->shutdown(_manager); }

	protected:
		PipelineManager *_manager;
		PacketDescReceiver *_head;
		PipelineMembers _pipelineMembers;
		uint32_t _id;
};

// Pipeline for parsing NFS
class NfsPipeline : public ChroniclePipeline {
	public:
		NfsPipeline(PipelineManager *manager, uint32_t id, 
			OutputManager *outputModule);
		~NfsPipeline() { }
};

// Pipeline for generating pcap output with parsing
class PcapPipeline : public ChroniclePipeline {
	public:
		PcapPipeline(PipelineManager *manager, uint32_t id, int snapLen);
		~PcapPipeline() { }
};

#endif // CHRONICLE_PIPELINE_H

