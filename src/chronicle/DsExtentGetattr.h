// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTGETATTR_H
#define DSEXTENTGETATTR_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentGetattr : public DsExtentWriter {
public:
    DsExtentGetattr();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3ga;
    Int64Field _n3gaRecordId;
    Variable32Field _n3gaFilehandle;
    Int32Field _n3gaStatus;
    ByteField _n3gaTypeId;
    Variable32Field  _n3gaType;
    Int32Field _n3gaMode;
    Int32Field _n3gaNlink;
    Int32Field _n3gaUid;
    Int32Field _n3gaGid;
    Int64Field _n3gaSize;
    Int64Field _n3gaUsed;
    Int64Field _n3gaSpecdata;
    Int64Field _n3gaFsid;
    Int64Field _n3gaFileid;
    Int64Field _n3gaCtime;
    Int64Field _n3gaMtime;
    Int64Field _n3gaAtime;
};

#endif // DSEXTENTGETATTR_H
