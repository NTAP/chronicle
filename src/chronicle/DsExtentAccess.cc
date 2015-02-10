// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentAccess.h"
#include "DsWriter.h"

DsExtentAccess::DsExtentAccess() :
    _n3acRecordId(_esN3ac, "record_id"),
    _n3acFilehandle(_esN3ac, "filehandle"),
    _n3acAccess(_esN3ac, "access"),
    _n3acStatus(_esN3ac, "status"),
    _n3acPostopTypeId(_esN3ac, "postop_type_id", Field::flag_nullable),
    _n3acPostopType(_esN3ac, "postop_type", Field::flag_nullable),
    _n3acPostopMode(_esN3ac, "postop_mode", Field::flag_nullable),
    _n3acPostopNlink(_esN3ac, "postop_nlink", Field::flag_nullable),
    _n3acPostopUid(_esN3ac, "postop_uid", Field::flag_nullable),
    _n3acPostopGid(_esN3ac, "postop_gid", Field::flag_nullable),
    _n3acPostopSize(_esN3ac, "postop_size", Field::flag_nullable),
    _n3acPostopUsed(_esN3ac, "postop_used", Field::flag_nullable),
    _n3acPostopSpecdata(_esN3ac, "postop_specdata", Field::flag_nullable),
    _n3acPostopFsid(_esN3ac, "postop_fsid", Field::flag_nullable),
    _n3acPostopFileid(_esN3ac, "postop_fileid", Field::flag_nullable),
    _n3acPostopCtime(_esN3ac, "postop_ctime", Field::flag_nullable),
    _n3acPostopMtime(_esN3ac, "postop_mtime", Field::flag_nullable),
    _n3acPostopAtime(_esN3ac, "postop_atime", Field::flag_nullable),
    _n3acAccessRes(_esN3ac, "access_res", Field::flag_nullable)
{
}

void
DsExtentAccess::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr etN3ac(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_ACCESS));
    _om = new OutputModule(*outfile, _esN3ac, etN3ac, extentSize);
}

void
DsExtentAccess::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3acRecordId.set(call->dsRecordId);
    _n3acFilehandle.set(call->fileHandle, call->fhLen);
    _n3acAccess.set(call->accessMode);
    _n3acStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3acPostopTypeId.set(reply->type);
        _n3acPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3acPostopMode.set(0xFFF & reply->fileMode);
        _n3acPostopNlink.setNull(); // not parsed
        _n3acPostopUid.set(reply->fileUid);
        _n3acPostopGid.set(reply->fileGid);
        _n3acPostopSize.set(reply->fileSizeBytes);
        _n3acPostopUsed.set(reply->fileUsedBytes);
        _n3acPostopSpecdata.setNull(); // not parsed
        _n3acPostopFsid.set(reply->fileSystemId);
        _n3acPostopFileid.set(reply->fileId);
        _n3acPostopCtime.setNull(); // not parsed
        _n3acPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3acPostopAtime.setNull(); // not parsed
        _n3acAccessRes.set(reply->accessMode);
    } else {
        _n3acPostopTypeId.setNull();
        _n3acPostopType.setNull();
        _n3acPostopMode.setNull();
        _n3acPostopNlink.setNull();
        _n3acPostopUid.setNull();
        _n3acPostopGid.setNull();
        _n3acPostopSize.setNull();
        _n3acPostopUsed.setNull();
        _n3acPostopSpecdata.setNull();
        _n3acPostopFsid.setNull();
        _n3acPostopFileid.setNull();
        _n3acPostopCtime.setNull();
        _n3acPostopMtime.setNull();
        _n3acPostopAtime.setNull();
        _n3acAccessRes.setNull();
    }
}
