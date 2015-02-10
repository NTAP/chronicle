// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAP_PDU_WRITER_H
#define PCAP_PDU_WRITER_H

#include <pcap.h>
#include <string>
#include "OutputModule.h"

class PduDescriptor;
class PcapPacketBufferPool;

class PcapPduWriter : public ChronicleOutputModule {
	public:
		PcapPduWriter(ChronicleSource *src,  std::string baseName, 
			uint32_t id, int snapLength);
		~PcapPduWriter();
		void processRequest(ChronicleSource *src, PduDescriptor *pduDesc);
		void shutdown(ChronicleSource *src);

	private:
		class MsgBase;
		class MsgProcessRequest;
		class MsgShutdownProcess;
		
		void doProcessRequest(PduDescriptor *pduDesc);
		void doShutdownProcess();

		/// the filename for pcap trace (savefile)
		std::string _baseName;
		/// number of bytes written for this interface
		uint64_t _numBytesWritten;
		/// num packets received
		uint64_t _numPktsReceived;
		/// num packets released
		uint64_t _numPktsReleased;
		/// the source Chronicle process for this sink
		ChronicleSource *_source;
		/// pcap handle for savefile
		pcap_t *_pcapHandle;
		/// dump handle for savefile
		pcap_dumper_t *_pcapDumper;
		/// reference to packet buffer pool
		PcapPacketBufferPool *_bufPool;
		/// number of files written for this interface
		uint32_t _numFilesWritten;
		/// packet snapshot length
		int _snapLength;
		/// a flag to know when to write the pcap header file
		bool _pcapFileHeaderWritten;
};

#endif //PCAP_PDU_WRITER_H
