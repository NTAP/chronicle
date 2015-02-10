// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentLink.h"
#include "DsWriter.h"

DsExtentLink::DsExtentLink() :
    _n3lnRecordId(_esN3ln, "record_id"),
    _n3lnFilehandle(_esN3ln, "filehandle"),
    _n3lnDirfh(_esN3ln, "dirfh"),
    _n3lnFilename(_esN3ln, "filename"),
    _n3lnStatus(_esN3ln, "status"),
    _n3lnObjTypeId(_esN3ln, "obj_type_id", Field::flag_nullable),
    _n3lnObjType(_esN3ln, "obj_type", Field::flag_nullable),
    _n3lnObjMode(_esN3ln, "obj_mode", Field::flag_nullable),
    _n3lnObjNlink(_esN3ln, "obj_nlink", Field::flag_nullable),
    _n3lnObjUid(_esN3ln, "obj_uid", Field::flag_nullable),
    _n3lnObjGid(_esN3ln, "obj_gid", Field::flag_nullable),
    _n3lnObjSize(_esN3ln, "obj_size", Field::flag_nullable),
    _n3lnObjUsed(_esN3ln, "obj_used", Field::flag_nullable),
    _n3lnObjSpecdata(_esN3ln, "obj_specdata", Field::flag_nullable),
    _n3lnObjFsid(_esN3ln, "obj_fsid", Field::flag_nullable),
    _n3lnObjFileid(_esN3ln, "obj_fileid", Field::flag_nullable),
    _n3lnObjCtime(_esN3ln, "obj_ctime", Field::flag_nullable),
    _n3lnObjMtime(_esN3ln, "obj_mtime", Field::flag_nullable),
    _n3lnObjAtime(_esN3ln, "obj_atime", Field::flag_nullable),
    _n3lnParentPreopSize(_esN3ln, "parent_preop_size", Field::flag_nullable),
    _n3lnParentPreopCtime(_esN3ln, "parent_preop_ctime", Field::flag_nullable),
    _n3lnParentPreopMtime(_esN3ln, "parent_preop_mtime", Field::flag_nullable),
    _n3lnParentPostopTypeId(_esN3ln, "parent_postop_type_id", Field::flag_nullable),
    _n3lnParentPostopType(_esN3ln, "parent_postop_type", Field::flag_nullable),
    _n3lnParentPostopMode(_esN3ln, "parent_postop_mode", Field::flag_nullable),
    _n3lnParentPostopNlink(_esN3ln, "parent_postop_nlink", Field::flag_nullable),
    _n3lnParentPostopUid(_esN3ln, "parent_postop_uid", Field::flag_nullable),
    _n3lnParentPostopGid(_esN3ln, "parent_postop_gid", Field::flag_nullable),
    _n3lnParentPostopSize(_esN3ln, "parent_postop_size", Field::flag_nullable),
    _n3lnParentPostopUsed(_esN3ln, "parent_postop_used", Field::flag_nullable),
    _n3lnParentPostopSpecdata(_esN3ln, "parent_postop_specdata", Field::flag_nullable),
    _n3lnParentPostopFsid(_esN3ln, "parent_postop_fsid", Field::flag_nullable),
    _n3lnParentPostopFileid(_esN3ln, "parent_postop_fileid", Field::flag_nullable),
    _n3lnParentPostopCtime(_esN3ln, "parent_postop_ctime", Field::flag_nullable),
    _n3lnParentPostopMtime(_esN3ln, "parent_postop_mtime", Field::flag_nullable),
    _n3lnParentPostopAtime(_esN3ln, "parent_postop_atime", Field::flag_nullable)
{
}

void
DsExtentLink::attach(DataSeriesSink *outfile,
                     ExtentTypeLibrary *lib,
                     unsigned extentSize)
{
    ExtentType::Ptr
        etN3ln(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_LINK));
    _om = new OutputModule(*outfile, _esN3ln, etN3ln, extentSize);
}

void
DsExtentLink::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3lnRecordId.set(call->dsRecordId);
    _n3lnFilehandle.set(call->fileHandle, call->fhLen);
    NfsV3PduDescriptor::Link link;
    call->getLink(&link);
    _n3lnDirfh.set(link.dirFileHandle, call->fhLen);
    const char *lname = reinterpret_cast<const char *>(link.linkName);
    _n3lnFilename.set(lname, strnlen(lname, NFS_MAXNAMLEN+1));
    _n3lnStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3lnObjTypeId.set(reply->type);
        _n3lnObjType.set(DsWriter::typeIdToName(reply->type));
        _n3lnObjMode.set(0xFFF & reply->fileMode);
        _n3lnObjNlink.setNull(); // not parsed
        _n3lnObjUid.set(reply->fileUid);
        _n3lnObjGid.set(reply->fileGid);
        _n3lnObjSize.set(reply->fileSizeBytes);
        _n3lnObjUsed.set(reply->fileUsedBytes);
        _n3lnObjSpecdata.setNull(); // not parsed
        _n3lnObjFsid.set(reply->fileSystemId);
        _n3lnObjFileid.set(reply->fileId);
        _n3lnObjCtime.setNull(); // not parsed
        _n3lnObjMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3lnObjAtime.setNull(); // not parsed
        _n3lnParentPreopSize.setNull(); // not parsed
        _n3lnParentPreopCtime.setNull(); // not parsed
        _n3lnParentPreopMtime.setNull(); // not parsed
        _n3lnParentPostopTypeId.setNull(); // not parsed
        _n3lnParentPostopType.setNull(); // not parsed
        _n3lnParentPostopMode.setNull(); // not parsed
        _n3lnParentPostopNlink.setNull(); // not parsed
        _n3lnParentPostopUid.setNull(); // not parsed
        _n3lnParentPostopGid.setNull(); // not parsed
        _n3lnParentPostopSize.setNull(); // not parsed
        _n3lnParentPostopUsed.setNull(); // not parsed
        _n3lnParentPostopSpecdata.setNull(); // not parsed
        _n3lnParentPostopFsid.setNull(); // not parsed
        _n3lnParentPostopFileid.setNull(); // not parsed
        _n3lnParentPostopCtime.setNull(); // not parsed
        _n3lnParentPostopMtime.setNull(); // not parsed
        _n3lnParentPostopAtime.setNull(); // not parsed
    } else {
        _n3lnObjTypeId.setNull();
        _n3lnObjType.setNull();
        _n3lnObjMode.setNull();
        _n3lnObjNlink.setNull();
        _n3lnObjUid.setNull();
        _n3lnObjGid.setNull();
        _n3lnObjSize.setNull();
        _n3lnObjUsed.setNull();
        _n3lnObjSpecdata.setNull();
        _n3lnObjFsid.setNull();
        _n3lnObjFileid.setNull();
        _n3lnObjCtime.setNull();
        _n3lnObjMtime.setNull();
        _n3lnObjAtime.setNull();
        _n3lnParentPreopSize.setNull();
        _n3lnParentPreopCtime.setNull();
        _n3lnParentPreopMtime.setNull();
        _n3lnParentPostopTypeId.setNull();
        _n3lnParentPostopType.setNull();
        _n3lnParentPostopMode.setNull();
        _n3lnParentPostopNlink.setNull();
        _n3lnParentPostopUid.setNull();
        _n3lnParentPostopGid.setNull();
        _n3lnParentPostopSize.setNull();
        _n3lnParentPostopUsed.setNull();
        _n3lnParentPostopSpecdata.setNull();
        _n3lnParentPostopFsid.setNull();
        _n3lnParentPostopFileid.setNull();
        _n3lnParentPostopCtime.setNull();
        _n3lnParentPostopMtime.setNull();
        _n3lnParentPostopAtime.setNull();
    }
}
