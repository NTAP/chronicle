// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef	OUTPUT_MANAGER_H 
#define OUTPUT_MANAGER_H

#include <ctime>
#include "Process.h"
#include "ChronicleProcess.h"

class Chronicle;
class DsWriter;
class PcapPduWriter;
class StatListener;
class StatGatherer;
class AnalyticsManager;

/// A parent class for all the output modules (e.g., DsWriter, PcapPduWriter)
/// (note that the class OutputModule is already being used by DataSeries)
class ChronicleOutputModule : public Process, public PduDescReceiver {
	protected:
		uint32_t _id;
		AnalyticsManager *_analyticsManager;

	public:
		ChronicleOutputModule(std::string name, uint32_t id, 
				AnalyticsManager *analyticsManager = NULL)
			: Process(name), _id(id), _analyticsManager(analyticsManager) { }
		virtual ~ChronicleOutputModule() = 0;
		/// Return the unique output module ID
		std::string getId() { return std::to_string(_id); }
};

class OutputManager : public Process, public ChronicleSource {
	public:
		OutputManager(Chronicle *supervisor, uint8_t outputFormat, 
			uint32_t numOutputModules, int snapLength,
			AnalyticsManager *analyticsManager, time_t timestamp);
		~OutputManager() { }
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
		/**
		 * finds the output module for a particular flow
		 * @returns the ID for this flow
		 */
		PduDescReceiver *findNextModule(uint32_t pipelineId);
		/**
		 *  returns the module responsible for writing stats
		 *  @returns the module responsible for writing stats
		 */
		StatListener *getStatListener();
		/**
		 *  adds the output modules to the StatGather object
		 */
		void addOutputModules(StatGatherer *sg);
		
	private:
		class MsgBase;
		class MsgModuleDone;
		class MsgShutdownModules;
		class MsgHandleModuleShutdown;

		void doHandleModuleDone(ChronicleSink *sink);
		void doShutdownModules();
		void doHandleModuleShutdown(ChronicleSink *sink);

		/// output module table
		ChronicleOutputModule *_modules[MAX_OUTPUT_MODULE_NUM];
		/// supervisor
		Chronicle *_supervisor;
		/// process writing stats
		StatListener *_statListener;
		/// analytics module
		AnalyticsManager *_analyticsManager;
		/// number of modules
		uint32_t _numModules;
		/// number of killed modules
		uint32_t _killedModules;
		/// capture flag
		bool _disableCapture;
		/// output module format
		uint8_t _outputFormat;
};

#endif //OUTPUT_MANAGER_H
