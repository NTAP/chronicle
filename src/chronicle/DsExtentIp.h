// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTIP_H
#define DSEXTENTIP_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentIp : public DsExtentWriter {
public:
    DsExtentIp();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(PacketDescriptor *p, uint64_t recordId, bool isGoodPdu);
    
private:
    ExtentSeries _esIP;
    Variable32Field _ipInterface;
    ByteField _ipRingNum;
    Int64Field _ipPacketAt;
    Int32Field _ipSource;
    Int32Field _ipSourcePort;
    Variable32Field _ipSourceMac;
    Int32Field _ipDest;
    Int32Field _ipDestPort;
    Variable32Field _ipDestMac;
    Int32Field _ipWireLength;
    ByteField _ipProtocol;
    BoolField _ipIsFragment;
    Int32Field _ipTcpSeqnum;
    Int32Field _ipTcpPayloadLength;
    BoolField _ipIsGoodPdu;
    Int64Field _ipRecordId;
};

#endif // DSEXTENTIP_H
