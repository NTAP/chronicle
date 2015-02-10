// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTCOMMIT_H
#define DSEXTENTCOMMIT_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentCommit : public DsExtentWriter {
public:
    DsExtentCommit();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3cm;
    Int64Field _n3cmRecordId;
    Variable32Field _n3cmFile;
    Int64Field _n3cmOffset;
    Int32Field _n3cmCount;
    Int32Field _n3cmStatus;
    Int64Field _n3cmPreopSize;
    Int64Field _n3cmPreopCtime;
    Int64Field _n3cmPreopMtime;
    ByteField _n3cmPostopTypeId;
    Variable32Field  _n3cmPostopType;
    Int32Field _n3cmPostopMode;
    Int32Field _n3cmPostopNlink;
    Int32Field _n3cmPostopUid;
    Int32Field _n3cmPostopGid;
    Int64Field _n3cmPostopSize;
    Int64Field _n3cmPostopUsed;
    Int64Field _n3cmPostopSpecdata;
    Int64Field _n3cmPostopFsid;
    Int64Field _n3cmPostopFileid;
    Int64Field _n3cmPostopCtime;
    Int64Field _n3cmPostopMtime;
    Int64Field _n3cmPostopAtime;
    Variable32Field  _n3cmWriteverf;
};

#endif // DSEXTENTCOMMIT_H
