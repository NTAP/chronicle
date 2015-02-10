// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTREADLINK_H
#define DSEXTENTREADLINK_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentReadlink : public DsExtentWriter {
public:
    DsExtentReadlink();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3rl;
    Int64Field _n3rlRecordId;
    Variable32Field _n3rlFilehandle;
    Int32Field _n3rlStatus;
    ByteField _n3rlPostopTypeId;
    Variable32Field  _n3rlPostopType;
    Int32Field _n3rlPostopMode;
    Int32Field _n3rlPostopNlink;
    Int32Field _n3rlPostopUid;
    Int32Field _n3rlPostopGid;
    Int64Field _n3rlPostopSize;
    Int64Field _n3rlPostopUsed;
    Int64Field _n3rlPostopSpecdata;
    Int64Field _n3rlPostopFsid;
    Int64Field _n3rlPostopFileid;
    Int64Field _n3rlPostopCtime;
    Int64Field _n3rlPostopMtime;
    Int64Field _n3rlPostopAtime;
    Variable32Field _n3rlTarget;
};

#endif // DSEXTENTREADLINK_H
