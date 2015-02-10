// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTFSSTAT_H
#define DSEXTENTFSSTAT_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentFsstat : public DsExtentWriter {
public:
    DsExtentFsstat();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3fs;
    Int64Field _n3fsRecordId;
    Variable32Field _n3fsFilehandle;
    Int32Field _n3fsStatus;
    ByteField _n3fsTypeId;
    Variable32Field  _n3fsType;
    Int32Field _n3fsMode;
    Int32Field _n3fsNlink;
    Int32Field _n3fsUid;
    Int32Field _n3fsGid;
    Int64Field _n3fsSize;
    Int64Field _n3fsUsed;
    Int64Field _n3fsSpecdata;
    Int64Field _n3fsFsid;
    Int64Field _n3fsFileid;
    Int64Field _n3fsCtime;
    Int64Field _n3fsMtime;
    Int64Field _n3fsAtime;
    Int64Field _n3fsTbytes;
    Int64Field _n3fsFbytes;
    Int64Field _n3fsAbytes;
    Int64Field _n3fsTfiles;
    Int64Field _n3fsFfiles;
    Int64Field _n3fsAfiles;
    Int32Field _n3fsInvarsec;
};

#endif // DSEXTENTFSSTAT_H
