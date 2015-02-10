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
#include "PcapPduWriter.h"
#include "Message.h"
#include "ChronicleProcessRequest.h"
#include "PcapPacketBufferPool.h"
#include "PcapInterface.h"

class PcapPduWriter::MsgBase : public Message {
	protected:
		PcapPduWriter *_writer;

	public:
		MsgBase(PcapPduWriter *writer) : _writer(writer) { }
		virtual ~MsgBase() { }
};

class PcapPduWriter::MsgProcessRequest : public MsgBase {
	private:
		PduDescriptor *_pduDesc;

	public:
		MsgProcessRequest(PcapPduWriter *writer, 
			PduDescriptor *pduDesc) 
			: MsgBase(writer), _pduDesc(pduDesc) { }
		void run() { _writer->doProcessRequest(_pduDesc); }
};

class PcapPduWriter::MsgShutdownProcess : public MsgBase {
	public:
		MsgShutdownProcess(PcapPduWriter *writer) 
			: MsgBase(writer) { }
		void run() { _writer->doShutdownProcess(); }
};

PcapPduWriter::PcapPduWriter(ChronicleSource *src, std::string baseName,
		uint32_t id, int _snapLength) 
	: ChronicleOutputModule("PcapPduWriter", id), _source(src),
	_snapLength(_snapLength)
{
	_baseName = baseName;
	_pcapFileHeaderWritten = false;
	_numBytesWritten = _numFilesWritten = 0;
	_numPktsReceived = _numPktsReleased = 0;
	_pcapHandle = NULL;
	_pcapDumper= NULL;
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		_source->processDone(this);
}

PcapPduWriter::~PcapPduWriter()
{

}

void
PcapPduWriter::processRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
	enqueueMessage(new MsgProcessRequest(this, pduDesc));
}

void
PcapPduWriter::doProcessRequest(PduDescriptor *pduDesc)
{
	PduDescriptor *tmpPduDesc;
	PacketDescriptor *oldPktDesc;
	while (pduDesc) {
		PacketDescriptor *pktDesc = pduDesc->firstPktDesc;
		do {
			if(_pcapFileHeaderWritten && 
					_numBytesWritten + pktDesc->pcapHeader.caplen 
					> PCAP_MAX_TRACE_SIZE) { // pcap file has got too big
				pcap_dump_flush(_pcapDumper);
				pcap_dump_close(_pcapDumper);
				_numBytesWritten = 0;
				_pcapFileHeaderWritten = false;
			}

			if(!_pcapFileHeaderWritten) {
				_pcapHandle = pcap_open_dead(DLT_EN10MB, _snapLength);
				std::string fileName;
				char num[7];
				sprintf(num, "%06u", _numFilesWritten);
				fileName = _baseName + num + ".pcap";
				_pcapDumper = pcap_dump_open(_pcapHandle, fileName.c_str());
				if (_pcapDumper == NULL) {
					std::cout << "PcapPduWriter::doProcessRequest: pcap_dump_open: "
						<< pcap_geterr(_pcapHandle) << std::endl;
					_processState = ChronicleSink::CHRONICLE_ERR;
					_bufPool->releasePacketDescriptor(pktDesc);
					_source->processDone(this);
					return ;
				}
				_pcapFileHeaderWritten = true;
				_numFilesWritten++;
			}
			if (_processState == ChronicleSink::CHRONICLE_NORMAL) { // writing pcap header/data
				pcap_dump(reinterpret_cast<u_char *>(_pcapDumper), 
					&pktDesc->pcapHeader, 
					reinterpret_cast<u_char *> (pktDesc->getEthFrameAddress()));
				_numBytesWritten += pktDesc->pcapHeader.caplen;
			}
			oldPktDesc = pktDesc;
			pktDesc = pktDesc->next;
			_bufPool->releasePacketDescriptor(oldPktDesc);
		} while (oldPktDesc != pduDesc->lastPktDesc);
		tmpPduDesc = pduDesc;
		pduDesc = pduDesc->next;
		delete tmpPduDesc;
	}
}

void
PcapPduWriter::shutdown(ChronicleSource *src)
{
	if (src == _source)
		enqueueMessage(new MsgShutdownProcess(this));
}

void
PcapPduWriter::doShutdownProcess()
{
	if (_pcapDumper) {
		pcap_dump_flush(_pcapDumper);
		pcap_dump_close(_pcapDumper);
	}
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;
	printf("[module	%u] numPktsReceived:%lu numPktsReleased:%lu\n", 
		_id, _numPktsReceived, _numPktsReleased);
	_source->shutdownDone(this);
	exit();
}

