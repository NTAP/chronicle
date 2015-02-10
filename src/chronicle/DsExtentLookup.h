// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTLOOKUP_H
#define DSEXTENTLOOKUP_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentLookup : public DsExtentWriter {
public:
    DsExtentLookup();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3lu;
    Int64Field _n3luRecordId;
    Variable32Field _n3luDirfh;
    Variable32Field _n3luFilename;
    Int32Field _n3luStatus;
    Variable32Field _n3luFilehandle;
    ByteField _n3luObjTypeId;
    Variable32Field  _n3luObjType;
    Int32Field _n3luObjMode;
    Int32Field _n3luObjNlink;
    Int32Field _n3luObjUid;
    Int32Field _n3luObjGid;
    Int64Field _n3luObjSize;
    Int64Field _n3luObjUsed;
    Int64Field _n3luObjSpecdata;
    Int64Field _n3luObjFsid;
    Int64Field _n3luObjFileid;
    Int64Field _n3luObjCtime;
    Int64Field _n3luObjMtime;
    Int64Field _n3luObjAtime;
    ByteField _n3luParentTypeId;
    Variable32Field  _n3luParentType;
    Int32Field _n3luParentMode;
    Int32Field _n3luParentNlink;
    Int32Field _n3luParentUid;
    Int32Field _n3luParentGid;
    Int64Field _n3luParentSize;
    Int64Field _n3luParentUsed;
    Int64Field _n3luParentSpecdata;
    Int64Field _n3luParentFsid;
    Int64Field _n3luParentFileid;
    Int64Field _n3luParentCtime;
    Int64Field _n3luParentMtime;
    Int64Field _n3luParentAtime;
};

#endif // DSEXTENTLOOKUP_H
