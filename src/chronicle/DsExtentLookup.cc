// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentLookup.h"
#include "DsWriter.h"

DsExtentLookup::DsExtentLookup() :
    _n3luRecordId(_esN3lu, "record_id"),
    _n3luDirfh(_esN3lu, "dirfh"),
    _n3luFilename(_esN3lu, "filename"),
    _n3luStatus(_esN3lu, "status"),
    _n3luFilehandle(_esN3lu, "filehandle", Field::flag_nullable),
    _n3luObjTypeId(_esN3lu, "obj_type_id", Field::flag_nullable),
    _n3luObjType(_esN3lu, "obj_type", Field::flag_nullable),
    _n3luObjMode(_esN3lu, "obj_mode", Field::flag_nullable),
    _n3luObjNlink(_esN3lu, "obj_nlink", Field::flag_nullable),
    _n3luObjUid(_esN3lu, "obj_uid", Field::flag_nullable),
    _n3luObjGid(_esN3lu, "obj_gid", Field::flag_nullable),
    _n3luObjSize(_esN3lu, "obj_size", Field::flag_nullable),
    _n3luObjUsed(_esN3lu, "obj_used", Field::flag_nullable),
    _n3luObjSpecdata(_esN3lu, "obj_specdata", Field::flag_nullable),
    _n3luObjFsid(_esN3lu, "obj_fsid", Field::flag_nullable),
    _n3luObjFileid(_esN3lu, "obj_fileid", Field::flag_nullable),
    _n3luObjCtime(_esN3lu, "obj_ctime", Field::flag_nullable),
    _n3luObjMtime(_esN3lu, "obj_mtime", Field::flag_nullable),
    _n3luObjAtime(_esN3lu, "obj_atime", Field::flag_nullable),
    _n3luParentTypeId(_esN3lu, "parent_type_id", Field::flag_nullable),
    _n3luParentType(_esN3lu, "parent_type", Field::flag_nullable),
    _n3luParentMode(_esN3lu, "parent_mode", Field::flag_nullable),
    _n3luParentNlink(_esN3lu, "parent_nlink", Field::flag_nullable),
    _n3luParentUid(_esN3lu, "parent_uid", Field::flag_nullable),
    _n3luParentGid(_esN3lu, "parent_gid", Field::flag_nullable),
    _n3luParentSize(_esN3lu, "parent_size", Field::flag_nullable),
    _n3luParentUsed(_esN3lu, "parent_used", Field::flag_nullable),
    _n3luParentSpecdata(_esN3lu, "parent_specdata", Field::flag_nullable),
    _n3luParentFsid(_esN3lu, "parent_fsid", Field::flag_nullable),
    _n3luParentFileid(_esN3lu, "parent_fileid", Field::flag_nullable),
    _n3luParentCtime(_esN3lu, "parent_ctime", Field::flag_nullable),
    _n3luParentMtime(_esN3lu, "parent_mtime", Field::flag_nullable),
    _n3luParentAtime(_esN3lu, "parent_atime", Field::flag_nullable)
{
}

void
DsExtentLookup::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3lu(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_LOOKUP));
    _om = new OutputModule(*outfile, _esN3lu, etN3lu, extentSize);
}

void
DsExtentLookup::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3luRecordId.set(call->dsRecordId);
    _n3luDirfh.set(call->fileHandle, call->fhLen);
    char filename[NFS_MAXNAMLEN+1];
    call->getFileName(reinterpret_cast<unsigned char*>(filename),
                      call->miscIndex0, call->pktDesc[0]);
    _n3luFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
    _n3luStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3luFilehandle.set(reply->fileHandle, reply->fhLen);
        _n3luObjTypeId.set(reply->type);
        _n3luObjType.set(DsWriter::typeIdToName(reply->type));
        _n3luObjMode.set(0xFFF & reply->fileMode);
        _n3luObjNlink.setNull(); // not parsed
        _n3luObjUid.set(reply->fileUid);
        _n3luObjGid.set(reply->fileGid);
        _n3luObjSize.set(reply->fileSizeBytes);
        _n3luObjUsed.set(reply->fileUsedBytes);
        _n3luObjSpecdata.setNull(); // not parsed
        _n3luObjFsid.set(reply->fileSystemId);
        _n3luObjFileid.set(reply->fileId);
        _n3luObjCtime.setNull(); // not parsed
        _n3luObjMtime.set(reply->fileModTime);
        _n3luObjAtime.setNull(); // not parsed
        _n3luParentTypeId.setNull(); // not parsed
        _n3luParentType.setNull(); // not parsed
        _n3luParentMode.setNull(); // not parsed
        _n3luParentNlink.setNull(); // not parsed
        _n3luParentUid.setNull(); // not parsed
        _n3luParentGid.setNull(); // not parsed
        _n3luParentSize.setNull(); // not parsed
        _n3luParentUsed.setNull(); // not parsed
        _n3luParentSpecdata.setNull(); // not parsed
        _n3luParentFsid.setNull(); // not parsed
        _n3luParentFileid.setNull(); // not parsed
        _n3luParentCtime.setNull(); // not parsed
        _n3luParentMtime.setNull(); // not parsed
        _n3luParentAtime.setNull(); // not parsed
    } else {
        _n3luFilehandle.setNull();
        _n3luObjTypeId.setNull();
        _n3luObjType.setNull();
        _n3luObjMode.setNull();
        _n3luObjNlink.setNull();
        _n3luObjUid.setNull();
        _n3luObjGid.setNull();
        _n3luObjSize.setNull();
        _n3luObjUsed.setNull();
        _n3luObjSpecdata.setNull();
        _n3luObjFsid.setNull();
        _n3luObjFileid.setNull();
        _n3luObjCtime.setNull();
        _n3luObjMtime.setNull();
        _n3luObjAtime.setNull();
        _n3luParentTypeId.setNull();
        _n3luParentType.setNull();
        _n3luParentMode.setNull();
        _n3luParentNlink.setNull();
        _n3luParentUid.setNull();
        _n3luParentGid.setNull();
        _n3luParentSize.setNull();
        _n3luParentUsed.setNull();
        _n3luParentSpecdata.setNull();
        _n3luParentFsid.setNull();
        _n3luParentFileid.setNull();
        _n3luParentCtime.setNull();
        _n3luParentMtime.setNull();
        _n3luParentAtime.setNull();
    }
}
