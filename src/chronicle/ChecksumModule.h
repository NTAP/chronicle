// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHECKSUMMODULE_H
#define CHECKSUMMODULE_H

#include "Process.h"
#include "ChronicleProcess.h"
//#include <openssl/evp.h>
#include "MurmurHash3.h"

class OutputManager;

/**
 * The checksum module checksums NFS read/write data so that the data
 * hashes can be written out by the DsWriter.
 */
class ChecksumModule : public Process,
                       public PduDescReceiver {
public:
    ChecksumModule(ChronicleSource *src, uint32_t id, 
		OutputManager *outputManager);
    ~ChecksumModule();

    // from ChronicleSink
    virtual void shutdown(ChronicleSource *src);
    // from PduDescReceiver
    virtual void processRequest(ChronicleSource *src, PduDescriptor *pduDesc);
	std::string getId() { return std::to_string(_pipelineId); }

private:
    class MessageBase;
    class MessagePRPdu;
    class MessageShutdown;

    void doShutdown(ChronicleSource *src);
    void doProcessRequest(ChronicleSource *src, PduDescriptor *pduDesc);

    /// Checksum a single PDU descriptor
    void checksumPdu(NfsV3PduDescriptor *desc, uint64_t fileOffset);

    ChronicleSource *_source;
	OutputManager *_outputManager;
    MurmurHash3_x64_128_State _mhstate;
	uint32_t _pipelineId;
};

#endif // CHECKSUMMODULE_H
