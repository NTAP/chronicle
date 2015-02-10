// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTRENAME_H
#define DSEXTENTRENAME_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentRename : public DsExtentWriter {
public:
    DsExtentRename();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply);
    
private:
    ExtentSeries _esN3mv;
    Int64Field _n3mvRecordId;
    Variable32Field _n3mvFromDirfh;
    Variable32Field _n3mvFromFilename;
    Variable32Field _n3mvToDirfh;
    Variable32Field _n3mvToFilename;
    Int32Field _n3mvStatus;
    Int64Field _n3mvFromdirPreopSize;
    Int64Field _n3mvFromdirPreopCtime;
    Int64Field _n3mvFromdirPreopMtime;
    ByteField _n3mvFromdirPostopTypeId;
    Variable32Field  _n3mvFromdirPostopType;
    Int32Field _n3mvFromdirPostopMode;
    Int32Field _n3mvFromdirPostopNlink;
    Int32Field _n3mvFromdirPostopUid;
    Int32Field _n3mvFromdirPostopGid;
    Int64Field _n3mvFromdirPostopSize;
    Int64Field _n3mvFromdirPostopUsed;
    Int64Field _n3mvFromdirPostopSpecdata;
    Int64Field _n3mvFromdirPostopFsid;
    Int64Field _n3mvFromdirPostopFileid;
    Int64Field _n3mvFromdirPostopCtime;
    Int64Field _n3mvFromdirPostopMtime;
    Int64Field _n3mvFromdirPostopAtime;
    Int64Field _n3mvTodirPreopSize;
    Int64Field _n3mvTodirPreopCtime;
    Int64Field _n3mvTodirPreopMtime;
    ByteField _n3mvTodirPostopTypeId;
    Variable32Field  _n3mvTodirPostopType;
    Int32Field _n3mvTodirPostopMode;
    Int32Field _n3mvTodirPostopNlink;
    Int32Field _n3mvTodirPostopUid;
    Int32Field _n3mvTodirPostopGid;
    Int64Field _n3mvTodirPostopSize;
    Int64Field _n3mvTodirPostopUsed;
    Int64Field _n3mvTodirPostopSpecdata;
    Int64Field _n3mvTodirPostopFsid;
    Int64Field _n3mvTodirPostopFileid;
    Int64Field _n3mvTodirPostopCtime;
    Int64Field _n3mvTodirPostopMtime;
    Int64Field _n3mvTodirPostopAtime;
};

#endif // DSEXTENTRENAME_H
