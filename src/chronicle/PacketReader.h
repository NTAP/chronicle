// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PACKET_READER_H
#define PACKET_READER_H

#include <pthread.h>
#include <list>
#include "Process.h"
#include "ChronicleProcess.h"
#include "FDQueue.h"

class Chronicle;
class Interface;
class NetworkHeaderParser;

/**
 * A generic packet reader process that can handle different types of
 * interfaces. Each PacketReader instance handles one interface exclusively.
 */
class PacketReader {
	public:
		PacketReader(Chronicle *supervisor, Interface *i, bool basicPipeline);
		~PacketReader();
		/// activate the reader thread
		pthread_t *activate();
		/// deactivate the reader thread (join on the reader thread)
		int deactivate() { return pthread_join(_thread, NULL); }
		/// PacketReader thread main loop
		static void *run(void *arg);
		/// process FDQueue messages
		void handleFDQueue();
		/// read the interface
		void readInterface();
		/// close the interface
		void closeInterface();
		/// get the interface's stats
		void getInterfaceStat();
		/// kill the reader
		void killPacketReader();
		/** 
		 * retrieve the interface corresponding to this reader
		 * @returns the interface handled by this reader
		 */
		Interface *getInterface() { return _interface; }
		void shutdownDone(Process *next);
		void processDone(Process *next);
		int getFDQueueFd();
		/// pointer to the next process in the pipeline
		NetworkHeaderParser *_networkHdrParser;

	private:
		class PacketReadyCb;
		class PacketReaderQueueCb;
		class PacketReaderMsg;
		
		void doReadInterface();
		void doCloseInterface();
		void doGetInterfaceStat();
		void doShutdownInterface();
		void doKillPacketReader();
		void doHandleProcessDone();

		/// pointer to the supervisor
		Chronicle *_supervisor;
		/// pointer to the interface this reader manages
		Interface *_interface;
		/// queue for receiving messages from supervisor and NetworkHeaderParser
		FDQueue<PacketReaderMsg> *_selfFdQueue;
		/// callback for the communication queue
		PacketReaderQueueCb *_queueCb;
		/// pthread corresponding to this reader
		pthread_t _thread;
		/// basic pipeline flag
		bool _basicPipeline;
};

#endif //PACKET_READER_H
