// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTACCESS_H
#define DSEXTENTACCESS_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentAccess : public DsExtentWriter {
public:
    DsExtentAccess();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3ac;
    Int64Field _n3acRecordId;
    Variable32Field _n3acFilehandle;
    Int32Field _n3acAccess;
    Int32Field _n3acStatus;
    ByteField _n3acPostopTypeId;
    Variable32Field  _n3acPostopType;
    Int32Field _n3acPostopMode;
    Int32Field _n3acPostopNlink;
    Int32Field _n3acPostopUid;
    Int32Field _n3acPostopGid;
    Int64Field _n3acPostopSize;
    Int64Field _n3acPostopUsed;
    Int64Field _n3acPostopSpecdata;
    Int64Field _n3acPostopFsid;
    Int64Field _n3acPostopFileid;
    Int64Field _n3acPostopCtime;
    Int64Field _n3acPostopMtime;
    Int64Field _n3acPostopAtime;
    Int32Field _n3acAccessRes;
};

#endif // DSEXTENTACCESS_H
