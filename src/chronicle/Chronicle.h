// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLE_H
#define CHRONICLE_H

#include <list>
#include <string>
#include "Process.h"
#include "FDQueue.h"

class Interface;
class InterfaceStat;
class PipelineManager;
class OutputManager;
class AnalyticsManager;
class PacketReader;
class PacketReaderFactory;
class CommandChannel;
class StatGatherer;
class StatListener;

/**
 * This process admits a number of interfaces for packet capture
 * and instantiates/manages a number of PacketReaders, one
 * per interface.
 */
class Chronicle : public Process {
	public:
		/// Interface stat report callback
		class InterfaceStatCb {
			public:
				virtual ~InterfaceStatCb() { }
				/// The callback action
				virtual void operator()(InterfaceStat *s) = 0;
		};

		/// Chronicle completion callback
		class CompletionCb {
			public:
				virtual ~CompletionCb() { }
				/// The callback action
				virtual void operator()(Chronicle *c, InterfaceStatCb *icb) = 0;
		};

		Chronicle(uint32_t numInterfaces, int channelFd, uint8_t pipelineType,
			uint32_t pipelineNum, uint8_t outputFormat,
			uint32_t numOutputManagers, int snapLength, bool analyticsEnabled,
			CompletionCb *compCb, InterfaceStatCb *statCb);
		~Chronicle();
		/**
		 * Initiates the Chronicle shutdown
		 */
		void startShutdown();
		/**
		 * Adds an interface to Chronicle (supervisor)
		 * @param[in] i the input interface
		 */
		void addInterface(Interface *i);
		/**
		 * Called by a PacketReader whenever it encounters an error for an interface
		 * @param[in] r The pointer to the reader experiencing error
		 */
		void handleReaderDone(PacketReader *r);
		/**
		 * Called by a PacketReader whenever it is done
		 * @param[in] r The pointer to the reader who completes its job
		 * @param[in] s The statistics being reported
		 */
		void handleReaderShutdown(PacketReader *r, InterfaceStat *s);
		/**
		 * Called by a PacketReader to report stats for an interface
		 * @param[in] r The pointer to the reader who reports stat
		 * @param[in] s The statistics being reported
		 */
		void handleReaderStat(PacketReader *r, InterfaceStat *s);
		/**
		 * Called by the PipelineManager to signal the termination of pipelines
		 */
		void handlePipelineManagerShutdown();
		/**
		 * Called by the OutputManager to signal the termination of output modules
		 */
		void handleOutputManagerShutdown();
		/**
		 * Called by the AnalyticsManager to signal the termination of 
		 * analytics modules
		 */
		void handleAnalyticsManagerShutdown();
		/**
		 * Called by the CommandChannel to report a commands sent via CLI
		 * @param[in] command The issued command
		 * @param[in] args The arguments for the command
		 */
		void handleCommandChannel(int command, std::list<std::string> args);
		/**
		 * Called by the CommandChannel to report it's done
		 */
		void handleDeadChannel();
		/**
		 * Finds the PacketReader corresponding to an interface
		 * @param[in] interfaceName The interface name
		 */
		PacketReader *findLiveReader(const char *interfaceName);
		/**
		 * Finds stats corresponding to a done interface
		 * @param[in] interfaceName The interface name
		 */
		InterfaceStat *findDoneInterface(const char *interfaceName);
		/**
		 * Handles events (PacketReader messages) notified by FDWatcher
		 */
		void handleFdWatcher();
		/**
		 * Used by the callback to print the stats for done interfaces
		 */
		std::list<InterfaceStat *> doneInterfaceStats;

    	class InterfaceListReceiver {
			public:
				virtual ~InterfaceListReceiver() { }
				virtual void receiveInterfaceList(const std::list<Interface *> &il) =
				0;
		};
		void getInterfaces(InterfaceListReceiver *ilr);
		void handleStatGathererShutdown();
		StatGatherer *getStatGatherer();
		std::string getId() { return "0"; }
	
	private:
		class MsgBase;
		class MsgStartShutdown;
		class MsgAddInterface;
		class MsgHandleFDWatcher;
		class MsgCloseReaders;
		class MsgPipelineManagerShutdown;
		class MsgOutputManagerShutdown;
		class MsgAnalyticsManagerShutdown;
		class MsgCommandChannel;
		class MsgDeadChannel;
		class MsgGetInterfaces;
    	class MsgStatGathererShutdown;
		class ChronicleQueueMsg;
		class ChronicleQueueCb;
		class FDWatcherCb;

		void doAddInterface(Interface *i);
		void doStartShutdown();
		void doShutdownReader(PacketReader *r);
		void doHandleReaderShutdown(PacketReader *r, InterfaceStat *s);
		void doCloseReaders();
		void doHandlePipelineManagerShutdown();
		void doHandleOutputManagerShutdown();
		void doHandleAnalyticsManagerShutdown();
		void doHandleCommandChannel(int command, std::list<std::string> args);
		void doHandleDeadChannel();
		void doGetReadersStat();
		void doHandleReaderStat(PacketReader *r, InterfaceStat *s);
		void doShutdownStatGatherer();
		void doHandleStatGathererShutdown();
		void doGetInterfaces(InterfaceListReceiver *ilr);
		void doHandleFdWatcher();

		/// number of interfaces handled by this supervisor
		uint32_t _numInterfaces;
		/// pipeline type
		uint8_t _pipelineType;
		/// a list of all live readers managed by the supervisor
		std::list<PacketReader *> _readers;
		/// a list of all done readers (shutdown is in progress)
		std::list<PacketReader *> _suspendedReaders;
		/// a list of all done readers (shutdown is over)
		std::list<PacketReader *> _doneReaders;
		/// pipeline manager
		PipelineManager *_pipelineManager;
		/// manager for output modules
		OutputManager *_outputManager;
		/// manager for analytics modules
		AnalyticsManager *_analyticsManager;
		/// callback for the supervisor
		CompletionCb *_compCb;
		/// interface stat callback
		InterfaceStatCb *_statCb;
		/// channel for external communication
		CommandChannel *_channel;
		/// queue for communication with PacketReaders
		FDQueue<ChronicleQueueMsg> *_selfFdQueue;
		/// file descriptor associated with _selfFdQueue
		int _queueFd;
		/// callback for the communication queue
		ChronicleQueueCb *_queueCb;
		/// FDWatcher callback
		FDWatcherCb *_fdWatcherCb;
		/// StatGatherer handle
		StatGatherer *_statModule;
		/// Graphite handle
		StatListener *_carbon;
		/// flag set when shutdown is in progress
		bool _shutdownStarted;
};

#endif //CHRONICLE_H
