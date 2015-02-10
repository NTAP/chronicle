// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentRename.h"
#include "DsWriter.h"

DsExtentRename::DsExtentRename() :
    _n3mvRecordId(_esN3mv, "record_id"),
    _n3mvFromDirfh(_esN3mv, "from_dirfh"),
    _n3mvFromFilename(_esN3mv, "from_filename"),
    _n3mvToDirfh(_esN3mv, "to_dirfh"),
    _n3mvToFilename(_esN3mv, "to_filename"),
    _n3mvStatus(_esN3mv, "status"),
    _n3mvFromdirPreopSize(_esN3mv, "fromdir_preop_size", Field::flag_nullable),
    _n3mvFromdirPreopCtime(_esN3mv, "fromdir_preop_ctime", Field::flag_nullable),
    _n3mvFromdirPreopMtime(_esN3mv, "fromdir_preop_mtime", Field::flag_nullable),
    _n3mvFromdirPostopTypeId(_esN3mv, "fromdir_postop_type_id", Field::flag_nullable),
    _n3mvFromdirPostopType(_esN3mv, "fromdir_postop_type", Field::flag_nullable),
    _n3mvFromdirPostopMode(_esN3mv, "fromdir_postop_mode", Field::flag_nullable),
    _n3mvFromdirPostopNlink(_esN3mv, "fromdir_postop_nlink", Field::flag_nullable),
    _n3mvFromdirPostopUid(_esN3mv, "fromdir_postop_uid", Field::flag_nullable),
    _n3mvFromdirPostopGid(_esN3mv, "fromdir_postop_gid", Field::flag_nullable),
    _n3mvFromdirPostopSize(_esN3mv, "fromdir_postop_size", Field::flag_nullable),
    _n3mvFromdirPostopUsed(_esN3mv, "fromdir_postop_used", Field::flag_nullable),
    _n3mvFromdirPostopSpecdata(_esN3mv, "fromdir_postop_specdata", Field::flag_nullable),
    _n3mvFromdirPostopFsid(_esN3mv, "fromdir_postop_fsid", Field::flag_nullable),
    _n3mvFromdirPostopFileid(_esN3mv, "fromdir_postop_fileid", Field::flag_nullable),
    _n3mvFromdirPostopCtime(_esN3mv, "fromdir_postop_ctime", Field::flag_nullable),
    _n3mvFromdirPostopMtime(_esN3mv, "fromdir_postop_mtime", Field::flag_nullable),
    _n3mvFromdirPostopAtime(_esN3mv, "fromdir_postop_atime", Field::flag_nullable),
    _n3mvTodirPreopSize(_esN3mv, "todir_preop_size", Field::flag_nullable),
    _n3mvTodirPreopCtime(_esN3mv, "todir_preop_ctime", Field::flag_nullable),
    _n3mvTodirPreopMtime(_esN3mv, "todir_preop_mtime", Field::flag_nullable),
    _n3mvTodirPostopTypeId(_esN3mv, "todir_postop_type_id", Field::flag_nullable),
    _n3mvTodirPostopType(_esN3mv, "todir_postop_type", Field::flag_nullable),
    _n3mvTodirPostopMode(_esN3mv, "todir_postop_mode", Field::flag_nullable),
    _n3mvTodirPostopNlink(_esN3mv, "todir_postop_nlink", Field::flag_nullable),
    _n3mvTodirPostopUid(_esN3mv, "todir_postop_uid", Field::flag_nullable),
    _n3mvTodirPostopGid(_esN3mv, "todir_postop_gid", Field::flag_nullable),
    _n3mvTodirPostopSize(_esN3mv, "todir_postop_size", Field::flag_nullable),
    _n3mvTodirPostopUsed(_esN3mv, "todir_postop_used", Field::flag_nullable),
    _n3mvTodirPostopSpecdata(_esN3mv, "todir_postop_specdata", Field::flag_nullable),
    _n3mvTodirPostopFsid(_esN3mv, "todir_postop_fsid", Field::flag_nullable),
    _n3mvTodirPostopFileid(_esN3mv, "todir_postop_fileid", Field::flag_nullable),
    _n3mvTodirPostopCtime(_esN3mv, "todir_postop_ctime", Field::flag_nullable),
    _n3mvTodirPostopMtime(_esN3mv, "todir_postop_mtime", Field::flag_nullable),
    _n3mvTodirPostopAtime(_esN3mv, "todir_postop_atime", Field::flag_nullable)
{
}

void
DsExtentRename::attach(DataSeriesSink *outfile,
                         ExtentTypeLibrary *lib,
                         unsigned extentSize)
{
    ExtentType::Ptr
        etN3mv(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_RENAME));
    _om = new OutputModule(*outfile, _esN3mv, etN3mv, extentSize);
}

void
DsExtentRename::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3mvRecordId.set(call->dsRecordId);
    _n3mvFromDirfh.set(call->fileHandle, call->fhLen);
    char filename[NFS_MAXNAMLEN+1];
    call->getFileName(reinterpret_cast<unsigned char*>(filename),
                      call->miscIndex0, call->pktDesc[0]);
    _n3mvFromFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
    
    call->getFileHandle(call->miscIndex2, call->pktDesc[2]);
    _n3mvToDirfh.set(call->fileHandle, call->fhLen);
    call->getFileName(reinterpret_cast<unsigned char*>(filename),
                      call->miscIndex1, call->pktDesc[1]);
    _n3mvToFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
    _n3mvStatus.set(reply->nfsStatus);
    // None of the attrs are currently parsed
    _n3mvFromdirPreopSize.setNull();
    _n3mvFromdirPreopCtime.setNull();
    _n3mvFromdirPreopMtime.setNull();
    _n3mvFromdirPostopTypeId.setNull();
    _n3mvFromdirPostopType.setNull();
    _n3mvFromdirPostopMode.setNull();
    _n3mvFromdirPostopNlink.setNull();
    _n3mvFromdirPostopUid.setNull();
    _n3mvFromdirPostopGid.setNull();
    _n3mvFromdirPostopSize.setNull();
    _n3mvFromdirPostopUsed.setNull();
    _n3mvFromdirPostopSpecdata.setNull();
    _n3mvFromdirPostopFsid.setNull();
    _n3mvFromdirPostopFileid.setNull();
    _n3mvFromdirPostopCtime.setNull();
    _n3mvFromdirPostopMtime.setNull();
    _n3mvFromdirPostopAtime.setNull();
    _n3mvTodirPreopSize.setNull();
    _n3mvTodirPreopCtime.setNull();
    _n3mvTodirPreopMtime.setNull();
    _n3mvTodirPostopTypeId.setNull();
    _n3mvTodirPostopType.setNull();
    _n3mvTodirPostopMode.setNull();
    _n3mvTodirPostopNlink.setNull();
    _n3mvTodirPostopUid.setNull();
    _n3mvTodirPostopGid.setNull();
    _n3mvTodirPostopSize.setNull();
    _n3mvTodirPostopUsed.setNull();
    _n3mvTodirPostopSpecdata.setNull();
    _n3mvTodirPostopFsid.setNull();
    _n3mvTodirPostopFileid.setNull();
    _n3mvTodirPostopCtime.setNull();
    _n3mvTodirPostopMtime.setNull();
    _n3mvTodirPostopAtime.setNull();
}
