// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTFSINFO_H
#define DSEXTENTFSINFO_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentFsinfo : public DsExtentWriter {
public:
    DsExtentFsinfo();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3fi;
    Int64Field _n3fiRecordId;
    Variable32Field _n3fiFilehandle;
    Int32Field _n3fiStatus;
    ByteField _n3fiTypeId;
    Variable32Field  _n3fiType;
    Int32Field _n3fiMode;
    Int32Field _n3fiNlink;
    Int32Field _n3fiUid;
    Int32Field _n3fiGid;
    Int64Field _n3fiSize;
    Int64Field _n3fiUsed;
    Int64Field _n3fiSpecdata;
    Int64Field _n3fiFsid;
    Int64Field _n3fiFileid;
    Int64Field _n3fiCtime;
    Int64Field _n3fiMtime;
    Int64Field _n3fiAtime;
    Int32Field _n3fiRtmax;
    Int32Field _n3fiRtpref;
    Int32Field _n3fiRtmult;
    Int32Field _n3fiWtmax;
    Int32Field _n3fiWtpref;
    Int32Field _n3fiWtmult;
    Int32Field _n3fiDtpref;
    Int64Field _n3fiMaxfilesize;
    Int64Field _n3fiTimeDelta;
    Int32Field _n3fiProperties;
};

#endif // DSEXTENTFSINFO_H
