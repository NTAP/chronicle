// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentRemove.h"
#include "DsWriter.h"

DsExtentRemove::DsExtentRemove() :
    _n3rmRecordId(_esN3rm, "record_id"),
    _n3rmDirfh(_esN3rm, "dirfh"),
    _n3rmFilename(_esN3rm, "filename"),
    _n3rmStatus(_esN3rm, "status"),
    _n3rmParentPreopSize(_esN3rm, "parent_preop_size", Field::flag_nullable),
    _n3rmParentPreopCtime(_esN3rm, "parent_preop_ctime", Field::flag_nullable),
    _n3rmParentPreopMtime(_esN3rm, "parent_preop_mtime", Field::flag_nullable),
    _n3rmParentPostopTypeId(_esN3rm, "parent_postop_type_id", Field::flag_nullable),
    _n3rmParentPostopType(_esN3rm, "parent_postop_type", Field::flag_nullable),
    _n3rmParentPostopMode(_esN3rm, "parent_postop_mode", Field::flag_nullable),
    _n3rmParentPostopNlink(_esN3rm, "parent_postop_nlink", Field::flag_nullable),
    _n3rmParentPostopUid(_esN3rm, "parent_postop_uid", Field::flag_nullable),
    _n3rmParentPostopGid(_esN3rm, "parent_postop_gid", Field::flag_nullable),
    _n3rmParentPostopSize(_esN3rm, "parent_postop_size", Field::flag_nullable),
    _n3rmParentPostopUsed(_esN3rm, "parent_postop_used", Field::flag_nullable),
    _n3rmParentPostopSpecdata(_esN3rm, "parent_postop_specdata", Field::flag_nullable),
    _n3rmParentPostopFsid(_esN3rm, "parent_postop_fsid", Field::flag_nullable),
    _n3rmParentPostopFileid(_esN3rm, "parent_postop_fileid", Field::flag_nullable),
    _n3rmParentPostopCtime(_esN3rm, "parent_postop_ctime", Field::flag_nullable),
    _n3rmParentPostopMtime(_esN3rm, "parent_postop_mtime", Field::flag_nullable),
    _n3rmParentPostopAtime(_esN3rm, "parent_postop_atime", Field::flag_nullable)
{
}

void
DsExtentRemove::attach(DataSeriesSink *outfile,
                         ExtentTypeLibrary *lib,
                         unsigned extentSize)
{
    ExtentType::Ptr
        etN3rm(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_REMOVE));
    _om = new OutputModule(*outfile, _esN3rm, etN3rm, extentSize);
}

void
DsExtentRemove::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3rmRecordId.set(call->dsRecordId);
    _n3rmDirfh.set(call->fileHandle, call->fhLen);
    char filename[NFS_MAXNAMLEN+1];
    call->getFileName(reinterpret_cast<unsigned char*>(filename),
                      call->miscIndex0, call->pktDesc[0]);
    _n3rmFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
    _n3rmStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3rmParentPreopSize.setNull(); // not parsed
        _n3rmParentPreopCtime.setNull(); // not parsed
        _n3rmParentPreopMtime.setNull(); // not parsed
        _n3rmParentPostopTypeId.set(reply->type);
        _n3rmParentPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3rmParentPostopMode.set(0xFFF & reply->fileMode);
        _n3rmParentPostopNlink.setNull(); // not parsed
        _n3rmParentPostopUid.set(reply->fileUid);
        _n3rmParentPostopGid.set(reply->fileGid);
        _n3rmParentPostopSize.set(reply->fileSizeBytes);
        _n3rmParentPostopUsed.set(reply->fileUsedBytes);
        _n3rmParentPostopSpecdata.setNull(); // not parsed
        _n3rmParentPostopFsid.set(reply->fileSystemId);
        _n3rmParentPostopFileid.set(reply->fileId);
        _n3rmParentPostopCtime.setNull(); // not parsed
        _n3rmParentPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3rmParentPostopAtime.setNull(); // not parsed
    } else {
        _n3rmParentPreopSize.setNull();
        _n3rmParentPreopCtime.setNull();
        _n3rmParentPreopMtime.setNull();
        _n3rmParentPostopTypeId.setNull();
        _n3rmParentPostopType.setNull();
        _n3rmParentPostopMode.setNull();
        _n3rmParentPostopNlink.setNull();
        _n3rmParentPostopUid.setNull();
        _n3rmParentPostopGid.setNull();
        _n3rmParentPostopSize.setNull();
        _n3rmParentPostopUsed.setNull();
        _n3rmParentPostopSpecdata.setNull();
        _n3rmParentPostopFsid.setNull();
        _n3rmParentPostopFileid.setNull();
        _n3rmParentPostopCtime.setNull();
        _n3rmParentPostopMtime.setNull();
        _n3rmParentPostopAtime.setNull();
    }
}
