// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentCommit.h"
#include "DsWriter.h"

DsExtentCommit::DsExtentCommit() :
    _n3cmRecordId(_esN3cm, "record_id"),
    _n3cmFile(_esN3cm, "file"),
    _n3cmOffset(_esN3cm, "offset"),
    _n3cmCount(_esN3cm, "count"),
    _n3cmStatus(_esN3cm, "status"),
    _n3cmPreopSize(_esN3cm, "preop_size", Field::flag_nullable),
    _n3cmPreopCtime(_esN3cm, "preop_ctime", Field::flag_nullable),
    _n3cmPreopMtime(_esN3cm, "preop_mtime", Field::flag_nullable),
    _n3cmPostopTypeId(_esN3cm, "postop_type_id", Field::flag_nullable),
    _n3cmPostopType(_esN3cm, "postop_type", Field::flag_nullable),
    _n3cmPostopMode(_esN3cm, "postop_mode", Field::flag_nullable),
    _n3cmPostopNlink(_esN3cm, "postop_nlink", Field::flag_nullable),
    _n3cmPostopUid(_esN3cm, "postop_uid", Field::flag_nullable),
    _n3cmPostopGid(_esN3cm, "postop_gid", Field::flag_nullable),
    _n3cmPostopSize(_esN3cm, "postop_size", Field::flag_nullable),
    _n3cmPostopUsed(_esN3cm, "postop_used", Field::flag_nullable),
    _n3cmPostopSpecdata(_esN3cm, "postop_specdata", Field::flag_nullable),
    _n3cmPostopFsid(_esN3cm, "postop_fsid", Field::flag_nullable),
    _n3cmPostopFileid(_esN3cm, "postop_fileid", Field::flag_nullable),
    _n3cmPostopCtime(_esN3cm, "postop_ctime", Field::flag_nullable),
    _n3cmPostopMtime(_esN3cm, "postop_mtime", Field::flag_nullable),
    _n3cmPostopAtime(_esN3cm, "postop_atime", Field::flag_nullable),
    _n3cmWriteverf(_esN3cm, "writeverf", Field::flag_nullable)
{
}

void
DsExtentCommit::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3cm(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_COMMIT));
    _om = new OutputModule(*outfile, _esN3cm, etN3cm, extentSize);
}

void
DsExtentCommit::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3cmRecordId.set(call->dsRecordId);
    _n3cmFile.set(call->fileHandle, call->fhLen);
    _n3cmOffset.set(call->fileOffset);
    _n3cmCount.set(call->byteCount);
    _n3cmStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3cmPreopSize.setNull(); // not parsed
        _n3cmPreopCtime.setNull(); // not parsed
        _n3cmPreopMtime.setNull(); // not parsed
        _n3cmPostopTypeId.set(reply->type);
        _n3cmPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3cmPostopMode.set(0xFFF & reply->fileMode);
        _n3cmPostopNlink.setNull(); // not parsed
        _n3cmPostopUid.set(reply->fileUid);
        _n3cmPostopGid.set(reply->fileGid);
        _n3cmPostopSize.set(reply->fileSizeBytes);
        _n3cmPostopUsed.set(reply->fileUsedBytes);
        _n3cmPostopSpecdata.setNull(); // not parsed
        _n3cmPostopFsid.set(reply->fileSystemId);
        _n3cmPostopFileid.set(reply->fileId);
        _n3cmPostopCtime.setNull(); // not parsed
        _n3cmPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3cmPostopAtime.setNull(); // not parsed
        _n3cmWriteverf.setNull(); // not parsed
    } else {
        _n3cmPreopSize.setNull();
        _n3cmPreopCtime.setNull();
        _n3cmPreopMtime.setNull();
        _n3cmPostopTypeId.setNull();
        _n3cmPostopType.setNull();
        _n3cmPostopMode.setNull();
        _n3cmPostopNlink.setNull();
        _n3cmPostopUid.setNull();
        _n3cmPostopGid.setNull();
        _n3cmPostopSize.setNull();
        _n3cmPostopUsed.setNull();
        _n3cmPostopSpecdata.setNull();
        _n3cmPostopFsid.setNull();
        _n3cmPostopFileid.setNull();
        _n3cmPostopCtime.setNull();
        _n3cmPostopMtime.setNull();
        _n3cmPostopAtime.setNull();
        _n3cmWriteverf.setNull();
    }
}
