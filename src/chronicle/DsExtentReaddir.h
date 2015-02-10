// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTREADDIR_H
#define DSEXTENTREADDIR_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentReaddir : public DsExtentWriter {
public:
    DsExtentReaddir();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    virtual void detach();
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    class DsExtentRDEntries: public DsExtentWriter {
    public:
        DsExtentRDEntries();
        virtual void attach(DataSeriesSink *outfile,
                            ExtentTypeLibrary *lib,
                            unsigned extentSize);
        void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    private:
        ExtentSeries _esN3de;
        Int64Field _n3deRecordId;
        Variable32Field _n3deFilename;
        Int64Field _n3deCookie;
        ByteField _n3deTypeId;
        Variable32Field  _n3deType;
        Int32Field _n3deMode;
        Int32Field _n3deNlink;
        Int32Field _n3deUid;
        Int32Field _n3deGid;
        Int64Field _n3deSize;
        Int64Field _n3deUsed;
        Int64Field _n3deSpecdata;
        Int64Field _n3deFsid;
        Int64Field _n3deFileid;
        Int64Field _n3deCtime;
        Int64Field _n3deMtime;
        Int64Field _n3deAtime;
        Variable32Field _n3deFilehandle;
    };

    DsExtentRDEntries _writerEntries;

    ExtentSeries _esN3rd;
    Int64Field _n3rdRecordId;
    Variable32Field _n3rdDir;
    Int64Field _n3rdCookie;
    Variable32Field _n3rdCookieverf;
    Int32Field _n3rdDircount;
    Int32Field _n3rdMaxcount;
    Int32Field _n3rdStatus;
    ByteField _n3rdDirTypeId;
    Variable32Field  _n3rdDirType;
    Int32Field _n3rdDirMode;
    Int32Field _n3rdDirNlink;
    Int32Field _n3rdDirUid;
    Int32Field _n3rdDirGid;
    Int64Field _n3rdDirSize;
    Int64Field _n3rdDirUsed;
    Int64Field _n3rdDirSpecdata;
    Int64Field _n3rdDirFsid;
    Int64Field _n3rdDirFileid;
    Int64Field _n3rdDirCtime;
    Int64Field _n3rdDirMtime;
    Int64Field _n3rdDirAtime;
    Variable32Field _n3rdNewCookieverf;
    BoolField _n3rdEof;
};

#endif // DSEXTENTREADDIR_H
