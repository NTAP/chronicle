// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLE_PROCESS_H
#define CHRONICLE_PROCESS_H

#include "ChronicleProcessRequest.h"

/*
 * Abstract base classes for different processes of the Chronicle pipeline
 * The main purpose of these class is to provide a common API for process 
 * setup, process teardown, and basic process communication along different
 * stages of the pipeline. 
 */

class ChronicleSink;

class ChronicleSource {
	public:
		ChronicleSource() { }
		virtual ~ChronicleSource() { }
		/**
		 * Used to signal that the next process in the pipeline has compeltely
		 * shutdown.
		 * @param[in] The sink process completing the shutdown
		 */
		virtual void shutdownDone(ChronicleSink *sink) = 0;
		/**
		 * Used to signal up the pipeline that a process is done (as a result
		 * of some kind of error).
		 * @param[in] The sink process issuing/passing the (error) signal
		 */
		virtual void processDone(ChronicleSink *sink) = 0;
};

class ChronicleSink {
	public:
		typedef enum processState {
			CHRONICLE_NORMAL,
			CHRONICLE_ERR
		} ChronicleProcessState;

		ChronicleSink() { _processState = CHRONICLE_NORMAL; }
		virtual ~ChronicleSink() { }
		/**
		 * Used to signal the next process in the pipeline to shut down.
		 * @param[in] The source process issuing the shutdown
		 */
		virtual void shutdown(ChronicleSource *source) = 0;

	protected:
		ChronicleProcessState _processState;
};
		
class PacketDescReceiver : public ChronicleSink {
	public:
		PacketDescReceiver() { }
		virtual ~PacketDescReceiver() { }
		virtual void processRequest(ChronicleSource *src, PacketDescriptor *pd) = 0;
};

class PduDescReceiver : public ChronicleSink {
	public:
		PduDescReceiver() { }
		virtual ~PduDescReceiver() { }
		virtual void processRequest(ChronicleSource *src, PduDescriptor *pduDesc) = 0;
};

class PacketPduDescReceiver : public ChronicleSink {
	public:
		PacketPduDescReceiver() { }
		virtual ~PacketPduDescReceiver() { }
		virtual void processRequest(ChronicleSource *src, PacketDescriptor *pd) = 0;
		virtual void processRequest(ChronicleSource *src, PduDescriptor *pduDesc) = 0;
};

#endif // CHRONICLE_PROCESS_H
