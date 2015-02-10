// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChecksumModule.h"
#include "OutputModule.h"
#include "Message.h"
#include <linux/nfs.h>
#include <linux/nfs3.h>
#include <iostream>

static const unsigned RPC_CALL = 0;
static const unsigned RPC_REPLY = 1;

class ChecksumModule::MessageBase : public Message {
protected:
    ChecksumModule *_self;
public:
    MessageBase(ChecksumModule *self) : _self(self) { }
};

ChecksumModule::ChecksumModule(ChronicleSource *src, uint32_t id,
		OutputManager *outputManager) :
    Process("ChecksumModule"), _source(src), _pipelineId(id)
{
	_outputManager = outputManager;
}

ChecksumModule::~ChecksumModule()
{

}

class ChecksumModule::MessagePRPdu: public MessageBase {
protected:
    ChronicleSource *_src;
    PduDescriptor *_pdu;
public:
    MessagePRPdu(ChecksumModule *self, ChronicleSource *src,
                 PduDescriptor *pdu) :
        MessageBase(self), _src(src), _pdu(pdu) { }
    void run() { _self->doProcessRequest(_src, _pdu); }
};

void
ChecksumModule::processRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
    enqueueMessage(new MessagePRPdu(this, src, pduDesc));
}

void
ChecksumModule::doProcessRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
    uint64_t fileOffset = 0;
    for (PduDescriptor *d = pduDesc; d != 0; d = d->next) {
        NfsV3PduDescriptor *nfsDesc = dynamic_cast<NfsV3PduDescriptor *>(d);
        if (nfsDesc && nfsDesc->parsable) {
            // relies on the fact that calls precede replies for the same op.
            if (nfsDesc->rpcMsgType == RPC_CALL) {
                fileOffset = nfsDesc->fileOffset;
            }
            if ((nfsDesc->rpcMsgType == RPC_CALL &&
                 nfsDesc->rpcProgramProcedure == NFS3PROC_WRITE) ||
                (nfsDesc->rpcMsgType == RPC_REPLY &&
                 nfsDesc->rpcProgramProcedure == NFS3PROC_READ &&
				 nfsDesc->nfsStatus == NFS_OK)) {
                // There should be data to xsum
                checksumPdu(nfsDesc, fileOffset);
            }
        }
    }

    // pass it on
	PduDescReceiver *next;
	next = _outputManager->findNextModule(_pipelineId);
	next->processRequest(src, pduDesc);
}

static void
initDataPointers(PacketDescriptor *pkt, unsigned char **payloadStart,
                 int *bytesLeft)
{
    *payloadStart = &pkt->packetBuffer[pkt->payloadOffset];
    *bytesLeft = pkt->payloadLength;
}

static bool
isFullPacket(PacketDescriptor *pkt)
{
    return pkt->pcapHeader.caplen == pkt->pcapHeader.len;
}

void
ChecksumModule::checksumPdu(NfsV3PduDescriptor *desc, uint64_t fileOffset)
{
    uint64_t md_value[2];

    int opBytesRemaining = desc->byteCount;
    PacketDescriptor *currentPacket = desc->pktDesc[0];
    if (!isFullPacket(currentPacket))
        return;
    unsigned char *currentData;
    int pktBytesRemaining;
    initDataPointers(currentPacket, &currentData, &pktBytesRemaining);
    // advance to start of xfer data
    int delta = desc->miscIndex1 - currentPacket->payloadOffset;
    currentData += delta;
    pktBytesRemaining -= delta;
    //std::cerr << "op @ " << fileOffset << std::endl;
    // advance to 512B offset
    if (fileOffset & 512llu) {
        delta = (fileOffset & ~512llu) + 512 - fileOffset;
        currentData += delta;
        pktBytesRemaining -= delta;
        opBytesRemaining -= delta;
        fileOffset += delta;
    }
    //std::cerr << "advanced to " << fileOffset << std::endl;
    while (opBytesRemaining >= 512) { // while more to checksum
        int xsumBytesRemaining = 512;
        MurmurHash3_x64_128_init(0, &_mhstate);
        while (xsumBytesRemaining) {
            int bytesThisCall = std::min(pktBytesRemaining, opBytesRemaining);
            bytesThisCall = std::min(bytesThisCall, xsumBytesRemaining);
            if (bytesThisCall <= 0) { // need a new packet
                // XXX advance to next packet
                currentPacket = currentPacket->next;
                if (!currentPacket || !isFullPacket(currentPacket)) {
                    return;
                }
                initDataPointers(currentPacket, &currentData,
                                 &pktBytesRemaining);
                if (bytesThisCall < 0) {
                    bytesThisCall = -bytesThisCall;
                    currentData += bytesThisCall;
                    fileOffset += bytesThisCall;
                }
                continue;
            }
            //std::cerr << "digesting bytes: " << bytesThisCall << std::endl;
            MurmurHash3_x64_128_update(currentData, bytesThisCall, &_mhstate);
            currentData += bytesThisCall;
            fileOffset += bytesThisCall;
            pktBytesRemaining -= bytesThisCall;
            opBytesRemaining -= bytesThisCall;
            xsumBytesRemaining -= bytesThisCall;
        }
        MurmurHash3_x64_128_finalize(&_mhstate, md_value);
        uint64_t hashvalue = md_value[0] ^ md_value[1];
        NfsV3PduDescriptor::ChecksumRecord rec(hashvalue, fileOffset - 512);
        //std::cerr << "checksum done: " << rec.offset << std::endl;
        desc->dataChecksums.push_back(rec);
    }
}

class ChecksumModule::MessageShutdown: public MessageBase {
protected:
    ChronicleSource *_src;
public:
    MessageShutdown(ChecksumModule *self, ChronicleSource *src) :
        MessageBase(self), _src(src) { }
    void run() { _self->doShutdown(_src); }
};

void
ChecksumModule::shutdown(ChronicleSource *src)
{
    enqueueMessage(new MessageShutdown(this, src));
}

void
ChecksumModule::doShutdown(ChronicleSource *src)
{
	_source->shutdownDone(this);
    exit();
}

