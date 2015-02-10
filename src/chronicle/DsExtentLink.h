// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTLINK_H
#define DSEXTENTLINK_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentLink : public DsExtentWriter {
public:
    DsExtentLink();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3ln;
    Int64Field _n3lnRecordId;
    Variable32Field _n3lnFilehandle;
    Variable32Field _n3lnDirfh;
    Variable32Field _n3lnFilename;
    Int32Field _n3lnStatus;
    ByteField _n3lnObjTypeId;
    Variable32Field  _n3lnObjType;
    Int32Field _n3lnObjMode;
    Int32Field _n3lnObjNlink;
    Int32Field _n3lnObjUid;
    Int32Field _n3lnObjGid;
    Int64Field _n3lnObjSize;
    Int64Field _n3lnObjUsed;
    Int64Field _n3lnObjSpecdata;
    Int64Field _n3lnObjFsid;
    Int64Field _n3lnObjFileid;
    Int64Field _n3lnObjCtime;
    Int64Field _n3lnObjMtime;
    Int64Field _n3lnObjAtime;
    Int64Field _n3lnParentPreopSize;
    Int64Field _n3lnParentPreopCtime;
    Int64Field _n3lnParentPreopMtime;
    ByteField _n3lnParentPostopTypeId;
    Variable32Field  _n3lnParentPostopType;
    Int32Field _n3lnParentPostopMode;
    Int32Field _n3lnParentPostopNlink;
    Int32Field _n3lnParentPostopUid;
    Int32Field _n3lnParentPostopGid;
    Int64Field _n3lnParentPostopSize;
    Int64Field _n3lnParentPostopUsed;
    Int64Field _n3lnParentPostopSpecdata;
    Int64Field _n3lnParentPostopFsid;
    Int64Field _n3lnParentPostopFileid;
    Int64Field _n3lnParentPostopCtime;
    Int64Field _n3lnParentPostopMtime;
    Int64Field _n3lnParentPostopAtime;
};

#endif // DSEXTENTLINK_H
