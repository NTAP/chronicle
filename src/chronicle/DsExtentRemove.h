// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTREMOVE_H
#define DSEXTENTREMOVE_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentRemove : public DsExtentWriter {
public:
    DsExtentRemove();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3rm;
    Int64Field _n3rmRecordId;
    Variable32Field _n3rmDirfh;
    Variable32Field _n3rmFilename;
    Int32Field _n3rmStatus;
    Int64Field _n3rmParentPreopSize;
    Int64Field _n3rmParentPreopCtime;
    Int64Field _n3rmParentPreopMtime;
    ByteField _n3rmParentPostopTypeId;
    Variable32Field  _n3rmParentPostopType;
    Int32Field _n3rmParentPostopMode;
    Int32Field _n3rmParentPostopNlink;
    Int32Field _n3rmParentPostopUid;
    Int32Field _n3rmParentPostopGid;
    Int64Field _n3rmParentPostopSize;
    Int64Field _n3rmParentPostopUsed;
    Int64Field _n3rmParentPostopSpecdata;
    Int64Field _n3rmParentPostopFsid;
    Int64Field _n3rmParentPostopFileid;
    Int64Field _n3rmParentPostopCtime;
    Int64Field _n3rmParentPostopMtime;
    Int64Field _n3rmParentPostopAtime;
};

#endif // DSEXTENTREMOVE_H
