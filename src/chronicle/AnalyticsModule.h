// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef	ANALYTICS_MODULE_H 
#define ANALYTICS_MODULE_H

#include <ctime>
#include <list>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include "ChronicleProcess.h"
#include "Process.h"
#include "PcapPacketBufferPool.h"

class Chronicle;
class StatGatherer;
class PcapPacketBufferPool;
class AnalyticsManager;

/// A parent class for all the analytics modules
class AnalyticsModule : public Process, public PduDescReceiver {
	public:
		AnalyticsModule(std::string name, AnalyticsManager *manager,
			std::string command, int id, int interval);
		virtual ~AnalyticsModule();
		virtual void report(struct timeval ts) = 0;
		std::string getId() { return std::to_string(_id); }
		std::string getCommand() { return _command; }
		std::string getOutputFile() { return _outputFile; }
	
	protected:
		void releasePdu(PduDescriptor *pduDesc);
		bool timeToReport(struct timeval ts);
		std::string getTime(struct timeval ts);

		/// analytics manager
		AnalyticsManager *_manager;
		/// reference to packet buffer pool
		PcapPacketBufferPool *_bufPool;
		/// command for this module
		std::string _command;
		/// output file
		std::string _outputFile;
		/// output file stream
		std::fstream _output;
		/// the start time of the module
		time_t _startTime;
		/// the report time of the module
		time_t _reportTime;
		/// analytics module id
		int _id;
		/// reporting interval
		int _interval;
};



class AnalyticsManager : public Process, public ChronicleSource, 
		public PduDescReceiver {
	public:
		AnalyticsManager(Chronicle *supervisor, bool analyticsEnabled, 
			time_t ts);
		~AnalyticsManager();
		void processRequest(ChronicleSource *src, PduDescriptor *pduDesc);
		/**
		 * invoked by the supervisor to pass the commands from CLI
		 * @param[in] src The supervisor
		 * @param[in] command The analytics command
		 */
		void processCommand(std::list<std::string> command);
		/**
		 * invoked when an output module has completely shut down
		 * @param[in] sink The process that has shut down
		 */
		void shutdownDone(ChronicleSink *sink);
		/**
		 * invoked when an output module has encountered an error and should be 
		 * shutdown
		 * @param[in] sink The process that is done
		 */
		void processDone(ChronicleSink *sink);
		/**
		 * invoked by the supervisor to shut down the output modules
		 */
		void shutdown();
		void shutdown(ChronicleSource *) { }
		/**
		 * triggered by StatGatherer to collect analytics modules process stats
		 */
		void getStats(StatGatherer *sg);

		static struct Nfs3OperationCounts {
			uint64_t packets;
			uint64_t getattr;
			uint64_t setattr;
			uint64_t lookup;
			uint64_t access;
			uint64_t readlink;
			uint64_t read;
			uint64_t write;
			uint64_t create;
			uint64_t mkdir;
			uint64_t symlink;
			uint64_t mknod;
			uint64_t remove;
			uint64_t rmdir;
			uint64_t rename;
			uint64_t link;
			uint64_t readdir;
			uint64_t readdirplus;
			uint64_t fsstat;
			uint64_t fsinfo;
			uint64_t pathconf;
			uint64_t commit;
		} nfs3OpCounts;

	private:
		class MsgBase;
		class MsgProcessRequest;
		class MsgProcessCommand;
		class MsgModuleDone;
		class MsgShutdownModules;
		class MsgHandleModuleShutdown;
		class MsgGetStats;
		
		void doProcessRequest(PduDescriptor *pdu);
		void updateNfsStats(PduDescriptor *pduDesc);
		void doProcessCommand(std::list<std::string> command);
		void doHandleModuleDone(ChronicleSink *sink);
		void doShutdownModules();
		void doHandleModuleShutdown(ChronicleSink *sink);
		void doHandleGetStats(StatGatherer *sg);
		void usage();

		/// reference to the supervisor
		Chronicle *_supervisor;
		/// a map of all the analytics modules
		std::map<int, AnalyticsModule *> _modules;
		/// reference to the packet buffer pool
		PcapPacketBufferPool *_bufPool;
		/// the current number of analytics modules
		int _numModules;
		/// the number of analytics modules terminated during shutdown
		int _killedModules;
		/// a monotonically increasing id for modules
		int _moduleId;
		/// flag to denote analytics is enabled
		bool _enabled;
		/// shutdown flag
		bool _shutdown;
		/// timestamp for this Chronicle run
		time_t _timestamp;
};

#endif //ANALYTICS_MODULE_H
