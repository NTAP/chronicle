// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAP_WRITER_H
#define PCAP_WRITER_H

#include <pcap.h>
#include "Process.h"
#include "ChronicleProcess.h"

class PacketDescriptor;
class PcapPacketBufferPool;

class PcapWriter : public Process, public PacketDescReceiver {
	public:
		PcapWriter(ChronicleSource *src, unsigned pipelineId, int snapLen);
		~PcapWriter();
		void processRequest(ChronicleSource *src, PacketDescriptor *pktDesc);
		void shutdown(ChronicleSource *src);

	private:
		class MsgBase;
		class MsgProcessRequest;
		class MsgShutdownProcess;
		
		void doProcessRequest(PacketDescriptor *pktDesc);
		void doShutdownProcess();

		/// the filename for pcap trace (savefile)
		char fileName[MAX_INTERFACE_NAME_LEN];
		/// number of bytes written for this interface
		uint64_t numBytesWritten;
		/// num packets received
		uint64_t numPktsReceived;
		/// num packets released
		uint64_t numPktsReleased;
		/// the source Chronicle process for this sink
		ChronicleSource *source;
		/// pcap handle for savefile
		pcap_t *pcapHandle;
		/// dump handle for savefile
		pcap_dumper_t *pcapDumper;
		/// reference to packet buffer pool
		PcapPacketBufferPool *bufPool;
		/// the pipeline in which this writer belongs
		unsigned pipelineId;
		/// number of files written for this interface
		uint32_t numFilesWritten;
		/// packet snapshot length
		int snapLength;
		/// a flag to know when to write the pcap header file
		bool pcapFileHeaderWritten;
};

#endif //PCAP_WRITER_H
