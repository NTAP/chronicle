// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentCreate.h"
#include "DsWriter.h"

DsExtentCreate::DsExtentCreate() :
    _n3crRecordId(_esN3cr, "record_id"),
    _n3crDirfh(_esN3cr, "dirfh"),
    _n3crFilename(_esN3cr, "filename"),
    _n3crCmode(_esN3cr, "cmode", Field::flag_nullable),
    _n3crTypeId(_esN3cr, "type_id"),
    _n3crType(_esN3cr, "type"),
    _n3crMode(_esN3cr, "mode", Field::flag_nullable),
    _n3crUid(_esN3cr, "uid", Field::flag_nullable),
    _n3crGid(_esN3cr, "gid", Field::flag_nullable),
    _n3crSize(_esN3cr, "size", Field::flag_nullable),
    _n3crMtime(_esN3cr, "mtime", Field::flag_nullable),
    _n3crAtime(_esN3cr, "atime", Field::flag_nullable),
    _n3crExclCreateverf(_esN3cr, "excl_createverf", Field::flag_nullable),
    _n3crTarget(_esN3cr, "target", Field::flag_nullable),
    _n3crSpecdata(_esN3cr, "specdata", Field::flag_nullable),
    _n3crStatus(_esN3cr, "status"),
    _n3crFilehandle(_esN3cr, "filehandle", Field::flag_nullable),
    _n3crObjTypeId(_esN3cr, "obj_type_id", Field::flag_nullable),
    _n3crObjType(_esN3cr, "obj_type", Field::flag_nullable),
    _n3crObjMode(_esN3cr, "obj_mode", Field::flag_nullable),
    _n3crObjNlink(_esN3cr, "obj_nlink", Field::flag_nullable),
    _n3crObjUid(_esN3cr, "obj_uid", Field::flag_nullable),
    _n3crObjGid(_esN3cr, "obj_gid", Field::flag_nullable),
    _n3crObjSize(_esN3cr, "obj_size", Field::flag_nullable),
    _n3crObjUsed(_esN3cr, "obj_used", Field::flag_nullable),
    _n3crObjSpecdata(_esN3cr, "obj_specdata", Field::flag_nullable),
    _n3crObjFsid(_esN3cr, "obj_fsid", Field::flag_nullable),
    _n3crObjFileid(_esN3cr, "obj_fileid", Field::flag_nullable),
    _n3crObjCtime(_esN3cr, "obj_ctime", Field::flag_nullable),
    _n3crObjMtime(_esN3cr, "obj_mtime", Field::flag_nullable),
    _n3crObjAtime(_esN3cr, "obj_atime", Field::flag_nullable),
    _n3crParentPreopSize(_esN3cr, "parent_preop_size", Field::flag_nullable),
    _n3crParentPreopCtime(_esN3cr, "parent_preop_ctime", Field::flag_nullable),
    _n3crParentPreopMtime(_esN3cr, "parent_preop_mtime", Field::flag_nullable),
    _n3crParentPostopTypeId(_esN3cr, "parent_postop_type_id", Field::flag_nullable),
    _n3crParentPostopType(_esN3cr, "parent_postop_type", Field::flag_nullable),
    _n3crParentPostopMode(_esN3cr, "parent_postop_mode", Field::flag_nullable),
    _n3crParentPostopNlink(_esN3cr, "parent_postop_nlink", Field::flag_nullable),
    _n3crParentPostopUid(_esN3cr, "parent_postop_uid", Field::flag_nullable),
    _n3crParentPostopGid(_esN3cr, "parent_postop_gid", Field::flag_nullable),
    _n3crParentPostopSize(_esN3cr, "parent_postop_size", Field::flag_nullable),
    _n3crParentPostopUsed(_esN3cr, "parent_postop_used", Field::flag_nullable),
    _n3crParentPostopSpecdata(_esN3cr, "parent_postop_specdata", Field::flag_nullable),
    _n3crParentPostopFsid(_esN3cr, "parent_postop_fsid", Field::flag_nullable),
    _n3crParentPostopFileid(_esN3cr, "parent_postop_fileid", Field::flag_nullable),
    _n3crParentPostopCtime(_esN3cr, "parent_postop_ctime", Field::flag_nullable),
    _n3crParentPostopMtime(_esN3cr, "parent_postop_mtime", Field::flag_nullable),
    _n3crParentPostopAtime(_esN3cr, "parent_postop_atime", Field::flag_nullable)
{
}

void
DsExtentCreate::attach(DataSeriesSink *outfile,
                         ExtentTypeLibrary *lib,
                         unsigned extentSize)
{
    ExtentType::Ptr
        etN3cr(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_CREATE));
    _om = new OutputModule(*outfile, _esN3cr, etN3cr, extentSize);
}

void
DsExtentCreate::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3crRecordId.set(call->dsRecordId);
    _n3crDirfh.set(call->fileHandle, call->fhLen);
    char filename[NFS_MAXNAMLEN+1];
    call->getFileName(reinterpret_cast<unsigned char*>(filename),
                      call->miscIndex0, call->pktDesc[0]);
    _n3crFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));

    // procedure-specific stuff
    char filetype = 0;
    char target[NFS_MAXNAMLEN+1];
    NfsV3PduDescriptor::DeviceSpec spec;
    switch (call->rpcProgramProcedure) {
    case NFS3PROC_CREATE:
        filetype = NF3REG;
        _n3crCmode.set(0); // not parsed
        _n3crMode.setNull(); // not parsed
        _n3crUid.setNull(); // not parsed
        _n3crGid.setNull(); // not parsed
        _n3crSize.setNull(); // not parsed
        _n3crMtime.setNull(); // not parsed
        _n3crAtime.setNull(); // not parsed
        _n3crExclCreateverf.setNull(); // not parsed
        _n3crTarget.setNull(); // not used
        _n3crSpecdata.setNull(); // not used
        break;
    case NFS3PROC_MKDIR:
        filetype = NF3DIR;
        _n3crCmode.setNull(); // not used
        _n3crMode.set(call->fileMode);
        _n3crUid.set(call->fileUid);
        _n3crGid.set(call->fileGid);
        _n3crSize.set(call->fileSizeBytes);
        _n3crMtime.set(call->fileModTime);
        _n3crAtime.setNull(); // not parsed
        _n3crExclCreateverf.setNull(); // not used
        _n3crTarget.setNull(); // not used
        _n3crSpecdata.setNull(); // not used
        break;
    case NFS3PROC_SYMLINK:
        filetype = NF3LNK;
        _n3crCmode.setNull(); // not used
        _n3crMode.set(call->fileMode);
        _n3crUid.set(call->fileUid);
        _n3crGid.set(call->fileGid);
        _n3crSize.set(call->fileSizeBytes);
        _n3crMtime.set(call->fileModTime);
        _n3crAtime.setNull(); // not parsed
        _n3crExclCreateverf.setNull(); // not used
        call->getFileName(reinterpret_cast<unsigned char*>(target),
                          call->miscIndex1, call->pktDesc[1]);
        _n3crTarget.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
        _n3crSpecdata.setNull(); // not used
        break;
    case NFS3PROC_MKNOD:
        filetype = call->type;
        _n3crCmode.setNull(); // not used
        _n3crMode.set(call->fileMode);
        _n3crUid.set(call->fileUid);
        _n3crGid.set(call->fileGid);
        _n3crSize.set(call->fileSizeBytes);
        _n3crMtime.set(call->fileModTime);
        _n3crAtime.setNull(); // not parsed
        _n3crExclCreateverf.setNull(); // not used
        _n3crTarget.setNull(); // not used
        if (filetype == NF3CHR || filetype == NF3BLK) {
            call->getDeviceSpec(&spec);
            _n3crSpecdata.set((static_cast<uint64_t>(spec.majorNum) << 32) +
                              spec.minorNum);
        } else {
            _n3crSpecdata.setNull();
        }
        break;
    }
    _n3crTypeId.set(filetype);
    _n3crType.set(DsWriter::typeIdToName(filetype));
    _n3crStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3crFilehandle.set(reply->fileHandle, reply->fhLen);
        _n3crObjTypeId.set(reply->type);
        _n3crObjType.set(DsWriter::typeIdToName(reply->type));
        _n3crObjMode.set(0xFFF & reply->fileMode);
        _n3crObjNlink.setNull(); // not parsed
        _n3crObjUid.set(reply->fileUid);
        _n3crObjGid.set(reply->fileGid);
        _n3crObjSize.set(reply->fileSizeBytes);
        _n3crObjUsed.set(reply->fileUsedBytes);
        _n3crObjSpecdata.setNull(); // not parsed
        _n3crObjFsid.set(reply->fileSystemId);
        _n3crObjFileid.set(reply->fileId);
        _n3crObjCtime.setNull(); // not parsed
        _n3crObjMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3crObjAtime.setNull(); // not parsed
        _n3crParentPreopSize.setNull(); // not parsed
        _n3crParentPreopCtime.setNull(); // not parsed
        _n3crParentPreopMtime.setNull(); // not parsed
        _n3crParentPostopTypeId.setNull(); // not parsed
        _n3crParentPostopType.setNull(); // not parsed
        _n3crParentPostopMode.setNull(); // not parsed
        _n3crParentPostopNlink.setNull(); // not parsed
        _n3crParentPostopUid.setNull(); // not parsed
        _n3crParentPostopGid.setNull(); // not parsed
        _n3crParentPostopSize.setNull(); // not parsed
        _n3crParentPostopUsed.setNull(); // not parsed
        _n3crParentPostopSpecdata.setNull(); // not parsed
        _n3crParentPostopFsid.setNull(); // not parsed
        _n3crParentPostopFileid.setNull(); // not parsed
        _n3crParentPostopCtime.setNull(); // not parsed
        _n3crParentPostopMtime.setNull(); // not parsed
        _n3crParentPostopAtime.setNull(); // not parsed
    } else {
        _n3crFilehandle.setNull();
        _n3crObjTypeId.setNull();
        _n3crObjType.setNull();
        _n3crObjMode.setNull();
        _n3crObjNlink.setNull();
        _n3crObjUid.setNull();
        _n3crObjGid.setNull();
        _n3crObjSize.setNull();
        _n3crObjUsed.setNull();
        _n3crObjSpecdata.setNull();
        _n3crObjFsid.setNull();
        _n3crObjFileid.setNull();
        _n3crObjCtime.setNull();
        _n3crObjMtime.setNull();
        _n3crObjAtime.setNull();
        _n3crParentPreopSize.setNull();
        _n3crParentPreopCtime.setNull();
        _n3crParentPreopMtime.setNull();
        _n3crParentPostopTypeId.setNull();
        _n3crParentPostopType.setNull();
        _n3crParentPostopMode.setNull();
        _n3crParentPostopNlink.setNull();
        _n3crParentPostopUid.setNull();
        _n3crParentPostopGid.setNull();
        _n3crParentPostopSize.setNull();
        _n3crParentPostopUsed.setNull();
        _n3crParentPostopSpecdata.setNull();
        _n3crParentPostopFsid.setNull();
        _n3crParentPostopFileid.setNull();
        _n3crParentPostopCtime.setNull();
        _n3crParentPostopMtime.setNull();
        _n3crParentPostopAtime.setNull();
    }
}
