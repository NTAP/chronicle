// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentSetattr.h"
#include "DsWriter.h"

DsExtentSetattr::DsExtentSetattr() :
    _n3saRecordId(_esN3sa, "record_id"),
    _n3saFilehandle(_esN3sa, "filehandle"),
    _n3saSetMode(_esN3sa, "set_mode", Field::flag_nullable),
    _n3saSetUid(_esN3sa, "set_uid", Field::flag_nullable),
    _n3saSetGid(_esN3sa, "set_gid", Field::flag_nullable),
    _n3saSetSize(_esN3sa, "set_size", Field::flag_nullable),
    _n3saSetMtime(_esN3sa, "set_mtime", Field::flag_nullable),
    _n3saSetAtime(_esN3sa, "set_atime", Field::flag_nullable),
    _n3saCtimeGuard(_esN3sa, "ctime_guard", Field::flag_nullable),
    _n3saStatus(_esN3sa, "status"),
    _n3saPreopSize(_esN3sa, "preop_size", Field::flag_nullable),
    _n3saPreopCtime(_esN3sa, "preop_ctime", Field::flag_nullable),
    _n3saPreopMtime(_esN3sa, "preop_mtime", Field::flag_nullable),
    _n3saPostopTypeId(_esN3sa, "postop_type_id", Field::flag_nullable),
    _n3saPostopType(_esN3sa, "postop_type", Field::flag_nullable),
    _n3saPostopMode(_esN3sa, "postop_mode", Field::flag_nullable),
    _n3saPostopNlink(_esN3sa, "postop_nlink", Field::flag_nullable),
    _n3saPostopUid(_esN3sa, "postop_uid", Field::flag_nullable),
    _n3saPostopGid(_esN3sa, "postop_gid", Field::flag_nullable),
    _n3saPostopSize(_esN3sa, "postop_size", Field::flag_nullable),
    _n3saPostopUsed(_esN3sa, "postop_used", Field::flag_nullable),
    _n3saPostopSpecdata(_esN3sa, "postop_specdata", Field::flag_nullable),
    _n3saPostopFsid(_esN3sa, "postop_fsid", Field::flag_nullable),
    _n3saPostopFileid(_esN3sa, "postop_fileid", Field::flag_nullable),
    _n3saPostopCtime(_esN3sa, "postop_ctime", Field::flag_nullable),
    _n3saPostopMtime(_esN3sa, "postop_mtime", Field::flag_nullable),
    _n3saPostopAtime(_esN3sa, "postop_atime", Field::flag_nullable)
{
}

void
DsExtentSetattr::attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize)
{
    ExtentType::Ptr
        etN3sa(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_SETATTR));
    _om = new OutputModule(*outfile, _esN3sa, etN3sa, extentSize);
}

void
DsExtentSetattr::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3saRecordId.set(call->dsRecordId);
    _n3saFilehandle.set(call->fileHandle, call->fhLen);
    _n3saSetMode.set(call->fileMode);
    _n3saSetUid.set(call->fileUid);
    _n3saSetGid.set(call->fileGid);
    _n3saSetSize.set(call->fileSizeBytes);
    _n3saSetMtime.set(call->fileModTime);
    _n3saSetAtime.setNull(); // not parsed
    _n3saStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3saPreopSize.setNull(); // not parsed
        _n3saPreopCtime.setNull(); // not parsed
        _n3saPreopMtime.setNull(); // not parsed
        _n3saPostopTypeId.set(reply->type);
        _n3saPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3saPostopMode.set(0xFFF & reply->fileMode);
        _n3saPostopNlink.setNull(); // not parsed
        _n3saPostopUid.set(reply->fileUid);
        _n3saPostopGid.set(reply->fileGid);
        _n3saPostopSize.set(reply->fileSizeBytes);
        _n3saPostopUsed.set(reply->fileUsedBytes);
        _n3saPostopSpecdata.setNull(); // not parsed
        _n3saPostopFsid.set(reply->fileSystemId);
        _n3saPostopFileid.set(reply->fileId);
        _n3saPostopCtime.setNull(); // not parsed
        _n3saPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3saPostopAtime.setNull(); // not parsed
    } else {
        _n3saPreopSize.setNull();
        _n3saPreopCtime.setNull();
        _n3saPreopMtime.setNull();
        _n3saPostopTypeId.setNull();
        _n3saPostopType.setNull();
        _n3saPostopMode.setNull();
        _n3saPostopNlink.setNull();
        _n3saPostopUid.setNull();
        _n3saPostopGid.setNull();
        _n3saPostopSize.setNull();
        _n3saPostopUsed.setNull();
        _n3saPostopSpecdata.setNull();
        _n3saPostopFsid.setNull();
        _n3saPostopFileid.setNull();
        _n3saPostopCtime.setNull();
        _n3saPostopMtime.setNull();
        _n3saPostopAtime.setNull();
    }
}
