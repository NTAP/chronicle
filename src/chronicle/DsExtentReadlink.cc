// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentReadlink.h"
#include "DsWriter.h"

DsExtentReadlink::DsExtentReadlink() :
    _n3rlRecordId(_esN3rl, "record_id"),
    _n3rlFilehandle(_esN3rl, "filehandle"),
    _n3rlStatus(_esN3rl, "status"),
    _n3rlPostopTypeId(_esN3rl, "postop_type_id", Field::flag_nullable),
    _n3rlPostopType(_esN3rl, "postop_type", Field::flag_nullable),
    _n3rlPostopMode(_esN3rl, "postop_mode", Field::flag_nullable),
    _n3rlPostopNlink(_esN3rl, "postop_nlink", Field::flag_nullable),
    _n3rlPostopUid(_esN3rl, "postop_uid", Field::flag_nullable),
    _n3rlPostopGid(_esN3rl, "postop_gid", Field::flag_nullable),
    _n3rlPostopSize(_esN3rl, "postop_size", Field::flag_nullable),
    _n3rlPostopUsed(_esN3rl, "postop_used", Field::flag_nullable),
    _n3rlPostopSpecdata(_esN3rl, "postop_specdata", Field::flag_nullable),
    _n3rlPostopFsid(_esN3rl, "postop_fsid", Field::flag_nullable),
    _n3rlPostopFileid(_esN3rl, "postop_fileid", Field::flag_nullable),
    _n3rlPostopCtime(_esN3rl, "postop_ctime", Field::flag_nullable),
    _n3rlPostopMtime(_esN3rl, "postop_mtime", Field::flag_nullable),
    _n3rlPostopAtime(_esN3rl, "postop_atime", Field::flag_nullable),
    _n3rlTarget(_esN3rl, "target", Field::flag_nullable)
{
}

void
DsExtentReadlink::attach(DataSeriesSink *outfile,
                         ExtentTypeLibrary *lib,
                         unsigned extentSize)
{
    ExtentType::Ptr
        etN3rl(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_READLINK));
    _om = new OutputModule(*outfile, _esN3rl, etN3rl, extentSize);
}

void
DsExtentReadlink::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3rlRecordId.set(call->dsRecordId);
    _n3rlFilehandle.set(call->fileHandle, call->fhLen);
    _n3rlStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3rlPostopTypeId.set(reply->type);
        _n3rlPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3rlPostopMode.set(0xFFF & reply->fileMode);
        _n3rlPostopNlink.setNull(); // not parsed
        _n3rlPostopUid.set(reply->fileUid);
        _n3rlPostopGid.set(reply->fileGid);
        _n3rlPostopSize.set(reply->fileSizeBytes);
        _n3rlPostopUsed.set(reply->fileUsedBytes);
        _n3rlPostopSpecdata.setNull(); // not parsed
        _n3rlPostopFsid.set(reply->fileSystemId);
        _n3rlPostopFileid.set(reply->fileId);
        _n3rlPostopCtime.setNull(); // not parsed
        _n3rlPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3rlPostopAtime.setNull(); // not parsed
        char target[NFS_MAXPATHLEN+1];
        reply->getFileName(reinterpret_cast<unsigned char*>(target),
                           reply->miscIndex0, reply->pktDesc[0]);
        _n3rlTarget.set(target, strnlen(target, NFS_MAXPATHLEN+1));
    } else {
        _n3rlPostopTypeId.setNull();
        _n3rlPostopType.setNull();
        _n3rlPostopMode.setNull();
        _n3rlPostopNlink.setNull();
        _n3rlPostopUid.setNull();
        _n3rlPostopGid.setNull();
        _n3rlPostopSize.setNull();
        _n3rlPostopUsed.setNull();
        _n3rlPostopSpecdata.setNull();
        _n3rlPostopFsid.setNull();
        _n3rlPostopFileid.setNull();
        _n3rlPostopCtime.setNull();
        _n3rlPostopMtime.setNull();
        _n3rlPostopAtime.setNull();
        _n3rlTarget.setNull();
    }
}
