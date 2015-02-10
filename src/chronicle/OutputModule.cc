// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <sstream>
#include "Chronicle.h"
#include "Message.h"
#include "OutputModule.h"
#include "PcapPduWriter.h"
#include "DsWriter.h"
#include "StatGatherer.h"
#include "AnalyticsModule.h"

ChronicleOutputModule::~ChronicleOutputModule()
{

}

class OutputManager::MsgBase : public Message {
	protected:
		OutputManager *_outputManager;
		
	public:
		MsgBase(OutputManager *outputManager) : _outputManager(outputManager) { }
		virtual ~MsgBase() { }
};

class OutputManager::MsgModuleDone : public MsgBase {
	private:
		ChronicleSink *_module;
	
	public:
		MsgModuleDone(OutputManager *outputManager, ChronicleSink *module) 
			: MsgBase(outputManager), _module(module) { }
		void run() { _outputManager->doHandleModuleDone(_module); }		
};

class OutputManager::MsgShutdownModules : public MsgBase {
	public:
		MsgShutdownModules(OutputManager *outputManager) 
			: MsgBase(outputManager) { }
		void run() { _outputManager->doShutdownModules(); }
};

class OutputManager::MsgHandleModuleShutdown : public MsgBase {
	private:
		ChronicleSink *_module;

	public:
		MsgHandleModuleShutdown(OutputManager *outputManager, ChronicleSink *module)
			: MsgBase(outputManager), _module(module) { }
		void run() { _outputManager->doHandleModuleShutdown(_module); }
};

OutputManager::OutputManager(Chronicle *supervisor, uint8_t outputFormat, 
	uint32_t numOutputModules, int snapLength, 
	AnalyticsManager *analyticsManager, time_t timestamp)
	: Process("OutputManager"), _supervisor(supervisor), 
	_numModules(numOutputModules), _killedModules(0), _disableCapture(false)
{
	_analyticsManager = analyticsManager;
	_outputFormat = outputFormat;
	_statListener = NULL;
	switch (_outputFormat) {
		case DS_OUTPUT:
			for (uint32_t i = 0; i < _numModules; i++) {
				std::ostringstream baseFileName;
				baseFileName << traceDirectory << "/chronicle_" << timestamp 
					<< "_p"	<< i << "_";
				_modules[i] = new DsWriter(this, baseFileName.str(), i,
					analyticsManager);
			}
			if (_numModules)
				_statListener = dynamic_cast<StatListener *> (_modules[0]);
			break;
		case PCAP_OUTPUT:
			for (uint32_t i = 0; i < _numModules; i++) {
				std::ostringstream baseFileName;
				baseFileName << traceDirectory << "/chronicle_" << timestamp 
					<< "_p" << i << "_";
				_modules[i] = new PcapPduWriter(this, baseFileName.str(), i, 
					snapLength);
			}
			break;
		default:
			_disableCapture = true;
	}
}

void 
OutputManager::shutdownDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgHandleModuleShutdown(this, sink));
}

void
OutputManager::doHandleModuleShutdown(ChronicleSink *sink)
{
	if ((_outputFormat != NO_OUTPUT && ++_killedModules == _numModules)
			|| _outputFormat == NO_OUTPUT) {
		_supervisor->handleOutputManagerShutdown();
		exit();
	}
}

void 
OutputManager::processDone(ChronicleSink *sink)
{
	enqueueMessage(new MsgModuleDone(this, sink));
}

void
OutputManager::doHandleModuleDone(ChronicleSink *sink)
{
	_supervisor->startShutdown();
}

void 
OutputManager::shutdown()
{
	enqueueMessage(new MsgShutdownModules(this));
}

void
OutputManager::doShutdownModules()
{
	if (_outputFormat == NO_OUTPUT) {
		doHandleModuleShutdown(NULL);
		return ;
	}
	for (uint32_t i = 0; i < _numModules; i++)
		_modules[i]->shutdown(this);
}

PduDescReceiver *
OutputManager::findNextModule(uint32_t pipelineId)
{
	if (_outputFormat != NO_OUTPUT && !_disableCapture)
		return _modules[pipelineId & (_numModules - 1)];
	return _analyticsManager;
}

StatListener *
OutputManager::getStatListener()
{
	return _statListener;
}

void
OutputManager::addOutputModules(StatGatherer *sg)
{
	if (_outputFormat == NO_OUTPUT)
		return;
	std::list<ChronicleOutputModule *> modules;
	for (uint32_t i = 0; i < _numModules; i++)
		modules.push_back(_modules[i]);
	sg->addOutputModules(modules);
}
