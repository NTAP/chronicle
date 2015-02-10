// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSWRITER_H
#define DSWRITER_H

#define DSWRITER_DEBUG_STATS  0

#include "Chronicle.h"
#include "DataSeries/DataSeriesFile.hpp"
#include "DataSeries/DataSeriesModule.hpp"
#include "DsExtentAccess.h"
#include "DsExtentCommit.h"
#include "DsExtentCreate.h"
#include "DsExtentFsinfo.h"
#include "DsExtentFsstat.h"
#include "DsExtentGetattr.h"
#include "DsExtentIp.h"
#include "DsExtentLink.h"
#include "DsExtentLookup.h"
#include "DsExtentPathconf.h"
#include "DsExtentRead.h"
#include "DsExtentReaddir.h"
#include "DsExtentReadlink.h"
#include "DsExtentRemove.h"
#include "DsExtentRename.h"
#include "DsExtentRpc.h"
#include "DsExtentSetattr.h"
#include "DsExtentStats.h"
#include "OutputModule.h"
#include "StatGatherer.h"

class Interface;
class AnalyticsManager;

class DsWriter : public ChronicleOutputModule,
                 public StatListener {
public:
    /// Create a writer for DataSeries files.
    /// @param[in] baseName The "basename" for the sequence of
    /// DataSeries files that will be written. The filenames will be
    /// "basename_0000.ds"
	/// @param[in] id The ID for this module
	/// @param[in] analyticsManager The reference to the analytics manager
    /// @param[in] cAlg Which compression mode to use when
    /// writing the file. Extent::compress_lzf is the fastest, and
    /// Extent::compress_bz2 is the most space efficient.
    DsWriter(ChronicleSource *src,
			std::string baseName,
			uint32_t id,
			AnalyticsManager *analyticsManager,
			int cAlg = Extent::compression_algs[Extent::compress_mode_lzf].compress_flag);
    ~DsWriter();

    // from ChronicleSink
    virtual void shutdown(ChronicleSource *src);
    // from PacketDescReceiver
    virtual void processRequest(ChronicleSource *src, PduDescriptor *pduDesc);

    static uint64_t tvToNs(const struct timeval &tv);
    static const char *typeIdToName(uint32_t type);
    static uint64_t nfstimeToNs(uint64_t nfstime);
    // from StatListener
    virtual void writeStats(uint64_t nsSinceEpoch,
                            const std::string &objectPath,
                            int64_t value);

private:
    class MessageBase;
    class MessagePRPdu;
    class MessageShutdown;
    class MessageWriteStatistics;
    class MessageWriteStats;

    class WriteCallback;

    void doShutdown(ChronicleSource *src);
    void doProcessRequest(ChronicleSource *src, PduDescriptor *pduDesc);
    void doProcessRequest(ChronicleSource *src, PacketDescriptor *pktDesc);
    void doProcessPacketList(PacketDescriptor *begin,
                             PacketDescriptor *end,
                             uint64_t rpcRecordId,
                             bool isGoodPdu);

    /// Writes out the underlying packets and frees both the packets &
    /// pdu descriptor
    void processPduDesc(PduDescriptor *pduDesc);

    virtual void doWriteStats(uint64_t nsSinceEpoch,
                              const std::string &objectPath,
                              int64_t value);

    /// Get the next filename in sequence.
    /// @returns the name of the next file to write
    std::string nextFilename();
    /// Close the current output file (if any).
    void closeFile();
    /// Open the next DS file for writing.
    void openNextFile();

    ChronicleSource *_source;
	std::string _baseName;
    unsigned _fileNumber;
    int _cAlg;

    DataSeriesSink *_dsFile;
    DsExtentIp _writerIP;
    DsExtentRpc _writerRpc;
    DsExtentGetattr _writerGetattr;
    DsExtentSetattr _writerSetattr;
    DsExtentLookup _writerLookup;
    DsExtentAccess _writerAccess;
    DsExtentReadlink _writerReadlink;
    DsExtentRead _writerRead;
    DsExtentCreate _writerCreate;
    DsExtentRemove _writerRemove;
    DsExtentRename _writerRename;
    DsExtentLink _writerLink;
    DsExtentReaddir _writerReaddir;
    DsExtentFsstat _writerFsstat;
    DsExtentFsinfo _writerFsinfo;
    DsExtentPathconf _writerPathconf;
    DsExtentCommit _writerCommit;
    DsExtentStats _writerStats;

    /// DataSeries ExtentWriteCallback so we can keep track of how big
    /// a file gets
    WriteCallback *_writeCb;
    /// The current offset into the DataSeries file, updated by the
    /// WriteCallback
    off64_t _currFileOffset;

    #if DSWRITER_DEBUG_STATS
    uint64_t pduCompleteCnt;
    uint64_t pduCompleteHdrCnt;
    uint64_t pduOtherCnt;
    uint64_t pduNonParsable;
    uint64_t pduParsableCnt;
    uint64_t pduParsableCompleteCnt;
    uint64_t pduParsableCompleteHdrCnt;
    uint64_t pduConsReplies;
    uint64_t pduCompUnmatchedCall;
    uint64_t pduCompHdrUnmatchedCall;
    uint64_t pduCompHdrMatchedCall;
    uint64_t pduCompMatchedCall;
    uint64_t pduUmatchedReply;
    uint64_t rpcConsReplies;
    #endif // DSWRITER_DEBUG_STATS
};

#endif // DSWRITER_H
