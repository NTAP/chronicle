// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTREAD_H
#define DSEXTENTREAD_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentRead: public DsExtentWriter {
public:
    DsExtentRead();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    virtual void detach();
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    class DsExtentChecksum: public DsExtentWriter {
    public:
        DsExtentChecksum();
        virtual void attach(DataSeriesSink *outfile,
                            ExtentTypeLibrary *lib,
                            unsigned extentSize);
        void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    private:
        ExtentSeries _esN3xs;
        Int64Field _n3xsRecordId;
        Int64Field _n3xsOffset;
        Int64Field  _n3xsChecksum;
    };
    
    ExtentSeries _esN3rw;
    Int64Field _n3rwRecordId;
    BoolField _n3rwIsRead;
    Variable32Field _n3rwFile;
    Int64Field _n3rwOffset;
    Int32Field _n3rwCount;
    ByteField _n3rwStable;
    Int32Field _n3rwStatus;
    Int64Field _n3rwPreopSize;
    Int64Field _n3rwPreopCtime;
    Int64Field _n3rwPreopMtime;
    ByteField _n3rwPostopTypeId;
    Variable32Field  _n3rwPostopType;
    Int32Field _n3rwPostopMode;
    Int32Field _n3rwPostopNlink;
    Int32Field _n3rwPostopUid;
    Int32Field _n3rwPostopGid;
    Int64Field _n3rwPostopSize;
    Int64Field _n3rwPostopUsed;
    Int64Field _n3rwPostopSpecdata;
    Int64Field _n3rwPostopFsid;
    Int64Field _n3rwPostopFileid;
    Int64Field _n3rwPostopCtime;
    Int64Field _n3rwPostopMtime;
    Int64Field _n3rwPostopAtime;
    Int32Field _n3rwCountActual;
    ByteField _n3rwStableRes;
    BoolField _n3rwEof;
    Variable32Field  _n3rwWriteverf;
    DsExtentChecksum _writerChecksum;
};

#endif // DSEXTENTREAD_H
