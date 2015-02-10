// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTPATHCONF_H
#define DSEXTENTPATHCONF_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentPathconf : public DsExtentWriter {
public:
    DsExtentPathconf();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3pc;
    Int64Field _n3pcRecordId;
    Variable32Field _n3pcFilehandle;
    Int32Field _n3pcStatus;
    ByteField _n3pcTypeId;
    Variable32Field  _n3pcType;
    Int32Field _n3pcMode;
    Int32Field _n3pcNlink;
    Int32Field _n3pcUid;
    Int32Field _n3pcGid;
    Int64Field _n3pcSize;
    Int64Field _n3pcUsed;
    Int64Field _n3pcSpecdata;
    Int64Field _n3pcFsid;
    Int64Field _n3pcFileid;
    Int64Field _n3pcCtime;
    Int64Field _n3pcMtime;
    Int64Field _n3pcAtime;
    Int32Field _n3pcLinkmax;
    Int32Field _n3pcNamemax;
    BoolField _n3pcNoTrunc;
    BoolField _n3pcChownRestricted;
    BoolField _n3pcCaseInsensitive;
    BoolField _n3pcCasePreserving;
};

#endif // DSEXTENTPATHCONF_H
