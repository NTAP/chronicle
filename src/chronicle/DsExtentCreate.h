// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTCREATE_H
#define DSEXTENTCREATE_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentCreate : public DsExtentWriter {
public:
    DsExtentCreate();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3cr;
    Int64Field _n3crRecordId;
    Variable32Field _n3crDirfh;
    Variable32Field _n3crFilename;
    ByteField _n3crCmode;
    ByteField _n3crTypeId;
    Variable32Field _n3crType;
    Int32Field _n3crMode;
    Int32Field _n3crUid;
    Int32Field _n3crGid;
    Int64Field _n3crSize;
    Int64Field _n3crMtime;
    Int64Field _n3crAtime;
    Variable32Field _n3crExclCreateverf;
    Variable32Field _n3crTarget;
    Int64Field _n3crSpecdata;
    Int32Field _n3crStatus;
    Variable32Field _n3crFilehandle;
    ByteField _n3crObjTypeId;
    Variable32Field  _n3crObjType;
    Int32Field _n3crObjMode;
    Int32Field _n3crObjNlink;
    Int32Field _n3crObjUid;
    Int32Field _n3crObjGid;
    Int64Field _n3crObjSize;
    Int64Field _n3crObjUsed;
    Int64Field _n3crObjSpecdata;
    Int64Field _n3crObjFsid;
    Int64Field _n3crObjFileid;
    Int64Field _n3crObjCtime;
    Int64Field _n3crObjMtime;
    Int64Field _n3crObjAtime;
    Int64Field _n3crParentPreopSize;
    Int64Field _n3crParentPreopCtime;
    Int64Field _n3crParentPreopMtime;
    ByteField _n3crParentPostopTypeId;
    Variable32Field  _n3crParentPostopType;
    Int32Field _n3crParentPostopMode;
    Int32Field _n3crParentPostopNlink;
    Int32Field _n3crParentPostopUid;
    Int32Field _n3crParentPostopGid;
    Int64Field _n3crParentPostopSize;
    Int64Field _n3crParentPostopUsed;
    Int64Field _n3crParentPostopSpecdata;
    Int64Field _n3crParentPostopFsid;
    Int64Field _n3crParentPostopFileid;
    Int64Field _n3crParentPostopCtime;
    Int64Field _n3crParentPostopMtime;
    Int64Field _n3crParentPostopAtime;
};

#endif // DSEXTENTCREATE_H
