// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <errno.h>
#include <cstring>
#include <iostream>
#include "PcapWriter.h"
#include "Message.h"
#include "ChronicleProcessRequest.h"
#include "PcapPacketBufferPool.h"
#include "PcapInterface.h"

class PcapWriter::MsgBase : public Message {
	protected:
		PcapWriter *_writer;

	public:
		MsgBase(PcapWriter *writer) : _writer(writer) { }
		virtual ~MsgBase() { }
};

class PcapWriter::MsgProcessRequest : public MsgBase {
	private:
		PacketDescriptor *_pktDesc;

	public:
		MsgProcessRequest(PcapWriter *writer, 
			PacketDescriptor *pktDesc) 
			: MsgBase(writer), _pktDesc(pktDesc) { }
		void run() { _writer->doProcessRequest(_pktDesc); }
};

class PcapWriter::MsgShutdownProcess : public MsgBase {
	public:
		MsgShutdownProcess(PcapWriter *writer) 
			: MsgBase(writer) { }
		void run() { _writer->doShutdownProcess(); }
};

PcapWriter::PcapWriter(ChronicleSource *src, unsigned pipelineId,
	int snapLength) 
	: source(src), pipelineId(pipelineId), snapLength(snapLength)
{
	pcapFileHeaderWritten = false;
	numBytesWritten = numFilesWritten = 0;
	numPktsReceived = numPktsReleased = 0;
	pcapHandle = NULL;
	pcapDumper= NULL;
	bufPool = PcapPacketBufferPool::registerBufferPool();
	if (bufPool == NULL)
		source->processDone(this);
}

PcapWriter::~PcapWriter()
{

}

void
PcapWriter::processRequest(ChronicleSource *src, PacketDescriptor *pktDesc)
{
	enqueueMessage(new MsgProcessRequest(this, pktDesc));
}

void
PcapWriter::doProcessRequest(PacketDescriptor *pktDesc)
{
	while (pktDesc != NULL) {
		++numPktsReceived;
		if(pcapFileHeaderWritten && 
				numBytesWritten + pktDesc->pcapHeader.caplen 
				> PCAP_MAX_TRACE_SIZE) { // pcap file has got too big
			pcap_dump_flush(pcapDumper);
			pcap_dump_close(pcapDumper);
			numBytesWritten = 0;
			pcapFileHeaderWritten = false;
		}

		if(!pcapFileHeaderWritten) {
			pcapHandle = pcap_open_dead(DLT_EN10MB, snapLength);
			sprintf(fileName, "%s/p%u-%u.pcap", traceDirectory.c_str(), 
				pipelineId, numFilesWritten);
			pcapDumper = pcap_dump_open(pcapHandle, fileName);
			if (pcapDumper == NULL) {
				std::cout << "PcapWriter::doProcessRequest: pcap_dump_open: "
					<< pcap_geterr(pcapHandle) << std::endl;
				_processState = ChronicleSink::CHRONICLE_ERR;
				bufPool->releasePacketDescriptor(pktDesc);
				source->processDone(this);
				return ;
			}
			pcapFileHeaderWritten = true;
			numFilesWritten++;
		}

		if (_processState == ChronicleSink::CHRONICLE_NORMAL) { // writing pcap header/data
			pcap_dump(reinterpret_cast<u_char *>(pcapDumper), 
				&pktDesc->pcapHeader, 
				reinterpret_cast<u_char *> (pktDesc->getEthFrameAddress()));
			numBytesWritten += pktDesc->pcapHeader.caplen;
		}
		PacketDescriptor *nextPktDesc = pktDesc->next;
		if (bufPool->releasePacketDescriptor(pktDesc))
			++numPktsReleased;
		pktDesc = nextPktDesc;
	}
}

void
PcapWriter::shutdown(ChronicleSource *src)
{
	if (src == source)
		enqueueMessage(new MsgShutdownProcess(this));
}

void
PcapWriter::doShutdownProcess()
{
	if (pcapDumper) {
		pcap_dump_flush(pcapDumper);
		pcap_dump_close(pcapDumper);
	}
	if (bufPool && bufPool->unregisterBufferPool()) 
		delete bufPool;
	printf("[pipeline %u] numPktsReceived:%lu numPktsReleased:%lu\n", 
		pipelineId, numPktsReceived, numPktsReleased);
	source->shutdownDone(this);
	exit();
}

