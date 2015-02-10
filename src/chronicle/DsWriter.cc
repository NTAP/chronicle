// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "DsWriter.h"
#include "Message.h"
#include "AnalyticsModule.h"
#include "ChronicleExtents.h"
#include <linux/nfs.h>
#include <linux/nfs3.h>
#include <arpa/inet.h>
#include <atomic>
#include <iomanip>
#include <netinet/in.h>

static std::atomic<uint64_t> recordId(0);
static const uint64_t STAT_INTERVAL = 10; // seconds

/**
 * Implements the DataSeries ExtentWriteCallback interface via the
 * functor. Unfortunately, DS makes a copy when the object is passed
 * in, so it's difficult to maintain multiple references to
 * it. Instead, we allow this object to be copyable, and have it
 * update the count in the main DsWriter object.
 */
class DsWriter::WriteCallback {
    off64_t *_currOffset;
public:
    WriteCallback(off64_t *offsetPtr) : _currOffset(offsetPtr) { }
    void operator()(off64_t offset, Extent &e) {
        *_currOffset = offset;
    }
    void reset() {
        *_currOffset = 0;
    }
};

const char *
DsWriter::typeIdToName(uint32_t type)
{
    const char *name = "unknown";
    switch (type) {
    case NF3REG: name = "regular"; break;
    case NF3DIR: name = "directory"; break;
    case NF3BLK: name = "block"; break;
    case NF3CHR: name = "character"; break;
    case NF3LNK: name = "symlink"; break;
    case NF3SOCK: name = "socket"; break;
    case NF3FIFO: name = "fifo"; break;
    }
    return name;
}

uint64_t
DsWriter::tvToNs(const struct timeval &tv)
{
    int64_t ts = static_cast<uint64_t>(tv.tv_sec) * 1000000000 +
        tv.tv_usec * 1000;
    return ts;
}

uint64_t
DsWriter::nfstimeToNs(uint64_t nfstime)
{
    uint64_t secs = nfstime >> 32;
    uint64_t nsecs = nfstime & 0xFFFFFFFFllu;
    return secs * 1000000000llu + nsecs;
}

class DsWriter::MessageBase : public Message {
protected:
    DsWriter *_self;
public:
    MessageBase(DsWriter *self) : _self(self) { }
};

DsWriter::DsWriter(ChronicleSource *src, std::string baseName, uint32_t id, 
		AnalyticsManager *analyticsManager, int cAlg) :
	ChronicleOutputModule("DsWriter", id, analyticsManager),
	_source(src),
    _baseName(baseName),
    _fileNumber(0),
    _cAlg(cAlg),
    _dsFile(0)
{
    #if DSWRITER_DEBUG_STATS
    pduCompleteCnt = pduCompleteHdrCnt = pduOtherCnt = pduNonParsable 
		= pduParsableCnt = pduParsableCompleteCnt=pduParsableCompleteHdrCnt
		= pduConsReplies = pduCompUnmatchedCall=pduCompHdrUnmatchedCall
		= pduCompHdrMatchedCall = pduCompMatchedCall = pduUmatchedReply
		= rpcConsReplies = 0;
    #endif // DSWRITER_DEBUG_STATS

    _writeCb = new WriteCallback(&_currFileOffset);
    openNextFile();
}

DsWriter::~DsWriter()
{
    closeFile();
    #if DSWRITER_DEBUG_STATS
    std::cout << "\n\n" <<
    "  pduCompleteCnt = " <<pduCompleteCnt <<
    "  pduCompleteHdrCnt = "<<pduCompleteHdrCnt<<
    "  pduOtherCnt = "<<pduOtherCnt <<
    "  pduNonParsable = " << pduNonParsable <<
    "  pduParsableCnt = "<<pduParsableCnt <<
    "  pduParsableCompleteCnt = "<<pduParsableCompleteCnt<<
    "  pduParsableCompleteHdrCnt = "<<pduParsableCompleteHdrCnt <<
    "  pduConsReplies = "<< pduConsReplies<<
    "  pduCompUnmatchedCall = "<<pduCompUnmatchedCall<<
    "  pduCompHdrUnmatchedCall = "<<pduCompHdrUnmatchedCall<<
    "  pduCompHdrMatchedCall = "<<pduCompHdrMatchedCall<<
    "  pduCompMatchedCall = "<<pduCompMatchedCall<<
    "  pduUmatchedReply = "<<pduUmatchedReply<<
    "  rpcConsReplies = "<<rpcConsReplies<<
    "\n\n";
    #endif // #DSWRITER_DEBUG_STATS
    delete _writeCb;
}

std::string
DsWriter::nextFilename()
{
    char num[7];
    sprintf(num, "%06u", _fileNumber);
    ++_fileNumber;
    return std::string(_baseName + num + ".ds");
}

static uint64_t
timens()
{
    timeval tv;
    gettimeofday(&tv, 0);
    return DsWriter::tvToNs(tv);
}

void
DsWriter::closeFile()
{
    uint64_t a = timens();
    if (dsEnableIpChecksum) {
        _writerIP.detach();
    }
    _writerRpc.detach();
    _writerGetattr.detach();
    _writerSetattr.detach();
    _writerLookup.detach();
    _writerAccess.detach();
    _writerReadlink.detach();
    _writerRead.detach();
    _writerCreate.detach();
    _writerRemove.detach();
    _writerRename.detach();
    _writerLink.detach();
    _writerReaddir.detach();
    _writerFsstat.detach();
    _writerFsinfo.detach();
    _writerPathconf.detach();
    _writerCommit.detach();
    _writerStats.detach();

    uint64_t b = timens();
    if (_dsFile) {
        _dsFile->flushPending();
        DataSeriesSink::Stats s = _dsFile->getStats();
        std::cout << "Closing DS file: " << _dsFile->getFilename()
                  << std::endl;
        s.printText(std::cout);
        delete _dsFile;
        _dsFile = NULL;
    }
    uint64_t c = timens();
    std::cout << "rotation times: "
              << "  free om: " << b-a
              << "  close file: " << c-b
              << std::endl;
}

void
DsWriter::openNextFile()
{
    closeFile();

    _dsFile = new DataSeriesSink(nextFilename(), _cAlg);
	//_dsFile->setCompressorCount(1); 
    _writeCb->reset();
    _dsFile->setExtentWriteCallback(*_writeCb);

    ExtentTypeLibrary lib;

    if (dsEnableIpChecksum) {
        _writerIP.attach(_dsFile, &lib, dsExtentSize);
    }
    _writerRpc.attach(_dsFile, &lib, dsExtentSize);
    _writerGetattr.attach(_dsFile, &lib, dsExtentSize);
    _writerSetattr.attach(_dsFile, &lib, dsExtentSize);
    _writerLookup.attach(_dsFile, &lib, dsExtentSize);
    _writerAccess.attach(_dsFile, &lib, dsExtentSize);
    _writerReadlink.attach(_dsFile, &lib, dsExtentSize);
    _writerRead.attach(_dsFile, &lib, dsExtentSize);
    _writerCreate.attach(_dsFile, &lib, dsExtentSize);
    _writerRemove.attach(_dsFile, &lib, dsExtentSize);
    _writerRename.attach(_dsFile, &lib, dsExtentSize);
    _writerLink.attach(_dsFile, &lib, dsExtentSize);
    _writerReaddir.attach(_dsFile, &lib, dsExtentSize);
    _writerFsstat.attach(_dsFile, &lib, dsExtentSize);
    _writerFsinfo.attach(_dsFile, &lib, dsExtentSize);
    _writerPathconf.attach(_dsFile, &lib, dsExtentSize);
    _writerCommit.attach(_dsFile, &lib, dsExtentSize);

    _writerStats.attach(_dsFile, &lib, dsExtentSize);

    _dsFile->writeExtentLibrary(lib);
}

class DsWriter::MessagePRPdu: public MessageBase {
protected:
    ChronicleSource *_src;
    PduDescriptor *_pdu;
public:
    MessagePRPdu(DsWriter *self, ChronicleSource *src,
                 PduDescriptor *pdu) :
        MessageBase(self), _src(src), _pdu(pdu) { }
    void run() { _self->doProcessRequest(_src, _pdu); }
};

void
DsWriter::processRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
    enqueueMessage(new MessagePRPdu(this, src, pduDesc));
}

void
DsWriter::doProcessRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
    const unsigned RPC_CALL = 0;
    const unsigned RPC_REPLY = 1;

    // PDU descriptor for most recent call
    NfsV3PduDescriptor *nfsCall = 0;

    // Descriptor for the most recent RPC call
    PduDescriptor *rpcCall = 0;

	// A reference to the first PDU descriptor
	PduDescriptor *firstPduDesc = pduDesc;

    #if DSWRITER_DEBUG_STATS
    PduDescriptor *rpcReply = 0;
    #endif

    while (pduDesc) {
        #if DSWRITER_DEBUG_STATS
            if ((PduDescriptor::PDU_BAD != pduDesc->rpcPduType) 
					&& (pduDesc->rpcMsgType == RPC_REPLY && rpcReply))
                rpcConsReplies++;
            if ((PduDescriptor::PDU_BAD != pduDesc->rpcPduType) 
					&& (pduDesc->rpcMsgType == RPC_REPLY))
                rpcReply = pduDesc;
        #endif

        #if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		pduDesc->tagPduPkts(PROCESS_DS_WRITER);
		#endif
        //pduDesc->dsRecordId = recordId++;
        bool processCurrentPdu = true;

        // Process the PDU
        if (PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType ||
            PduDescriptor::PDU_COMPLETE_HEADER == pduDesc->rpcPduType) {

                #if DSWRITER_DEBUG_STATS
                if(PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType)
					pduCompleteCnt++;
				if(PduDescriptor::PDU_COMPLETE_HEADER == pduDesc->rpcPduType)
					pduCompleteHdrCnt++;
                #endif

                if (pduDesc->rpcMsgType == RPC_CALL) {
                    if (rpcCall) {
                        _writerRpc.write(rpcCall, NULL);
                        #if DSWRITER_DEBUG_STATS
                        if(PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType)
							pduCompUnmatchedCall++;
						if(PduDescriptor::PDU_COMPLETE_HEADER == pduDesc->rpcPduType)
							pduCompHdrUnmatchedCall++;
                        #endif
                    }

                    pduDesc->dsRecordId = recordId++;
                    rpcCall = pduDesc;
                    #if DSWRITER_DEBUG_STATS
                    rpcReply = 0;
                    #endif
                }
                else if (pduDesc->rpcMsgType == RPC_REPLY) {
                    if (rpcCall) {
                        //Use same recordId for a req-reply pair
                        pduDesc->dsRecordId = rpcCall->dsRecordId;
                        _writerRpc.write(rpcCall, pduDesc);
                        rpcCall = 0;

                        #if DSWRITER_DEBUG_STATS
                        if(PduDescriptor::PDU_COMPLETE_HEADER == pduDesc->rpcPduType)
                            pduCompHdrMatchedCall++;
                        if(PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType)
                            pduCompMatchedCall++;
                        #endif
                    }
                    else {
                        //The request RPC for this reply is missing,
                        //so assign new recordId to it
                        pduDesc->dsRecordId = recordId++;
                        _writerRpc.write(NULL, pduDesc);
                        #if DSWRITER_DEBUG_STATS
                        pduUmatchedReply++;
                        #endif
                    }
                }
                else
                    assert(false);
        }
        else {
            pduDesc->dsRecordId = recordId++;
            #if DSWRITER_DEBUG_STATS
            pduOtherCnt++;
            #endif
        }

        NfsV3PduDescriptor *nfsPduDesc =
            dynamic_cast<NfsV3PduDescriptor *>(pduDesc);
        if (nfsPduDesc && nfsPduDesc->parsable) {
            #if DSWRITER_DEBUG_STATS
            pduParsableCnt++;
            if(PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType)
                pduParsableCompleteCnt++;
            if(PduDescriptor::PDU_COMPLETE_HEADER == pduDesc->rpcPduType)
                pduParsableCompleteHdrCnt++;
            #endif

            // if it's a call, hold onto it. The next PDU (in this
            // batch) should be the reply
            if (nfsPduDesc->rpcMsgType == RPC_CALL) {
                if (nfsCall) {
                    processPduDesc(nfsCall);
                }
                nfsCall = nfsPduDesc;
                processCurrentPdu = false;
            } else if (nfsPduDesc->rpcMsgType == RPC_REPLY && nfsCall) {
                // Process the NFS fields
                if (nfsPduDesc->rpcProgramProcedure ==
                    nfsCall->rpcProgramProcedure &&
                    nfsPduDesc->rpcXid == nfsCall->rpcXid) {
                    switch (pduDesc->rpcProgramProcedure) {
                    case NFS3PROC_NULL:
                        break;
                    case NFS3PROC_GETATTR:
                        _writerGetattr.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_SETATTR:
                        _writerSetattr.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_LOOKUP:
                        _writerLookup.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_ACCESS:
                        _writerAccess.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_READLINK:
                        _writerReadlink.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_READ:
                        _writerRead.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_WRITE:
                        _writerRead.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_CREATE:
                        _writerCreate.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_MKDIR:
                        _writerCreate.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_SYMLINK:
                        _writerCreate.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_MKNOD:
                        _writerCreate.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_REMOVE:
                        _writerRemove.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_RMDIR:
                        _writerRemove.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_RENAME:
                        _writerRename.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_LINK:
                        _writerLink.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_READDIR:
                        _writerReaddir.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_READDIRPLUS:
                        _writerReaddir.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_FSSTAT:
                        _writerFsstat.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_FSINFO:
                        _writerFsinfo.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_PATHCONF:
                        _writerPathconf.write(nfsCall, nfsPduDesc);
                        break;
                    case NFS3PROC_COMMIT:
                        _writerCommit.write(nfsCall, nfsPduDesc);
                        break;
                    }
                }
                processPduDesc(nfsCall);
                nfsCall = 0;
            }
            #if DSWRITER_DEBUG_STATS
            else{
                pduConsReplies++;
            }
            #endif
        } else {
            // the PDU wasn't parsable or wasn't NFS3. Since calls &
            // replies are paired, if we're currently holding onto a
            // call, this should have been the reply, so we should
            // free the call so that the IP packets in the
            // trace::net::ip extent will get written out in order
            if (nfsCall) {
                processPduDesc(nfsCall);
                nfsCall = 0;
            }
            #if DSWRITER_DEBUG_STATS
            pduNonParsable++;
            #endif
        }

        PduDescriptor *oldPduDesc = pduDesc;
        pduDesc = pduDesc->next;
        if (processCurrentPdu) {
            processPduDesc(oldPduDesc);
        }
    }

    // clean up if we had an unmatched NFS call
    if (nfsCall) {
        processPduDesc(nfsCall);
    }

    // We just added a record. See if it's time to rotate
    // the DS file.
    if (_currFileOffset > dsFileSize) {
        openNextFile();
    }

	_analyticsManager->processRequest(NULL, firstPduDesc);
}

void
DsWriter::doProcessRequest(ChronicleSource *src, PacketDescriptor *pktDesc)
{
    doProcessPacketList(pktDesc, NULL, static_cast<uint64_t>(-1), false);
}

void
DsWriter::doProcessPacketList(PacketDescriptor *begin,
                              PacketDescriptor *end,
                              uint64_t rpcRecordId,
                              bool isGoodPdu)
{
	PacketDescriptor *old;
	do {
        // write the DS packet record
        if (dsEnableIpChecksum) {
            _writerIP.write(begin, rpcRecordId, isGoodPdu);
        }

        // advance to next & delete the packet if we're the last reference
        old = begin;
        begin = begin->next;
	} while (old != end);
}

class DsWriter::MessageShutdown: public MessageBase {
protected:
    ChronicleSource *_src;
public:
    MessageShutdown(DsWriter *self, ChronicleSource *src) :
        MessageBase(self), _src(src) { }
    void run() { _self->doShutdown(_src); }
};

void
DsWriter::shutdown(ChronicleSource *src)
{
    enqueueMessage(new MessageShutdown(this, src));
}

void
DsWriter::doShutdown(ChronicleSource *src)
{
    src->shutdownDone(this);
    exit();
}

void
DsWriter::processPduDesc(PduDescriptor *pduDesc)
{
    PacketDescriptor *pktDesc = pduDesc->firstPktDesc;
    bool isGoodPdu = (PduDescriptor::PDU_COMPLETE == pduDesc->rpcPduType);
	doProcessPacketList(pktDesc, pduDesc->lastPktDesc, pduDesc->dsRecordId,
                        isGoodPdu); 
}


class DsWriter::MessageWriteStats: public MessageBase {
protected:
    uint64_t _ns;
    const std::string _path;
    int64_t _value;
public:
    MessageWriteStats(DsWriter *self, uint64_t ns, const std::string &path,
                      int64_t value) :
        MessageBase(self), _ns(ns), _path(path), _value(value) { }
    void run() { _self->doWriteStats(_ns, _path, _value); }
};

void
DsWriter::writeStats(uint64_t nsSinceEpoch,
                     const std::string &objectPath,
                     int64_t value)
{
    enqueueMessage(new MessageWriteStats(this, nsSinceEpoch, objectPath,
                                         value));
}

void
DsWriter::doWriteStats(uint64_t nsSinceEpoch,
                       const std::string &objectPath,
                       int64_t value)
{
    /*
    std::cout << nsSinceEpoch << ", "
              << objectPath << ", "
              << value << std::endl;
    */
    _writerStats.write(nsSinceEpoch, objectPath, value);
}
