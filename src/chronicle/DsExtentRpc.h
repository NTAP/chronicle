// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTRPC_H
#define DSEXTENTRPC_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentRpc : public DsExtentWriter {
public:
    DsExtentRpc();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(PduDescriptor *p, PduDescriptor *q);

private:
    ExtentSeries _esRpc;
    Int64Field _rpcRequestAt;
    Int64Field _rpcReplyAt;
    Int32Field _rpcClient;
    Int32Field _rpcClientPort;
    Int32Field _rpcServer;
    Int32Field _rpcServerPort;
    BoolField _rpcIsUdp;
    Int32Field _rpcRpcProgram;
    Int32Field _rpcProgramVersion;
    Int32Field _rpcProgramProc;
    Variable32Field _rpcOperation;
    Int32Field _rpcTransactionId;
    Int32Field _rpcCallHeaderOffset;
    Int32Field _rpcCallPayloadLength;
    Int32Field _rpcReplyHeaderOffset;
    Int32Field _rpcReplyPayloadLength;
    Int64Field _rpcRecordId;
};

#endif // DSEXTENTRPC_H
