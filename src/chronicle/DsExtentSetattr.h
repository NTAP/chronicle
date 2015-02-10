// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTSETATTR_H
#define DSEXTENTSETATTR_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentSetattr : public DsExtentWriter {
public:
    DsExtentSetattr();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3sa;
    Int64Field _n3saRecordId;
    Variable32Field _n3saFilehandle;
    Int32Field _n3saSetMode;
    Int32Field _n3saSetUid;
    Int32Field _n3saSetGid;
    Int64Field _n3saSetSize;
    Int64Field _n3saSetMtime;
    Int64Field _n3saSetAtime;
    Int64Field _n3saCtimeGuard;
    Int32Field _n3saStatus;
    Int64Field _n3saPreopSize;
    Int64Field _n3saPreopCtime;
    Int64Field _n3saPreopMtime;
    ByteField _n3saPostopTypeId;
    Variable32Field  _n3saPostopType;
    Int32Field _n3saPostopMode;
    Int32Field _n3saPostopNlink;
    Int32Field _n3saPostopUid;
    Int32Field _n3saPostopGid;
    Int64Field _n3saPostopSize;
    Int64Field _n3saPostopUsed;
    Int64Field _n3saPostopSpecdata;
    Int64Field _n3saPostopFsid;
    Int64Field _n3saPostopFileid;
    Int64Field _n3saPostopCtime;
    Int64Field _n3saPostopMtime;
    Int64Field _n3saPostopAtime;
};

#endif // DSEXTENTSETATTR_H
