// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentFsstat.h"
#include "DsWriter.h"

DsExtentFsstat::DsExtentFsstat() :
    _n3fsRecordId(_esN3fs, "record_id"),
    _n3fsFilehandle(_esN3fs, "filehandle"),
    _n3fsStatus(_esN3fs, "status"),
    _n3fsTypeId(_esN3fs, "type_id", Field::flag_nullable),
    _n3fsType(_esN3fs, "type", Field::flag_nullable),
    _n3fsMode(_esN3fs, "mode", Field::flag_nullable),
    _n3fsNlink(_esN3fs, "nlink", Field::flag_nullable),
    _n3fsUid(_esN3fs, "uid", Field::flag_nullable),
    _n3fsGid(_esN3fs, "gid", Field::flag_nullable),
    _n3fsSize(_esN3fs, "size", Field::flag_nullable),
    _n3fsUsed(_esN3fs, "used", Field::flag_nullable),
    _n3fsSpecdata(_esN3fs, "specdata", Field::flag_nullable),
    _n3fsFsid(_esN3fs, "fsid", Field::flag_nullable),
    _n3fsFileid(_esN3fs, "fileid", Field::flag_nullable),
    _n3fsCtime(_esN3fs, "ctime", Field::flag_nullable),
    _n3fsMtime(_esN3fs, "mtime", Field::flag_nullable),
    _n3fsAtime(_esN3fs, "atime", Field::flag_nullable),
    _n3fsTbytes(_esN3fs, "tbytes", Field::flag_nullable),
    _n3fsFbytes(_esN3fs, "fbytes", Field::flag_nullable),
    _n3fsAbytes(_esN3fs, "abytes", Field::flag_nullable),
    _n3fsTfiles(_esN3fs, "tfiles", Field::flag_nullable),
    _n3fsFfiles(_esN3fs, "ffiles", Field::flag_nullable),
    _n3fsAfiles(_esN3fs, "afiles", Field::flag_nullable),
    _n3fsInvarsec(_esN3fs, "invarsec", Field::flag_nullable)
{
}

void
DsExtentFsstat::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3fs(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_FSSTAT));
    _om = new OutputModule(*outfile, _esN3fs, etN3fs, extentSize);
}

void
DsExtentFsstat::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3fsRecordId.set(call->dsRecordId);
    _n3fsFilehandle.set(call->fileHandle, call->fhLen);
    _n3fsStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3fsTypeId.set(reply->type);
        _n3fsType.set(DsWriter::typeIdToName(reply->type));
        _n3fsMode.set(0xFFF & reply->fileMode);
        _n3fsNlink.setNull(); // not parsed
        _n3fsUid.set(reply->fileUid);
        _n3fsGid.set(reply->fileGid);
        _n3fsSize.set(reply->fileSizeBytes);
        _n3fsUsed.set(reply->fileUsedBytes);
        _n3fsSpecdata.setNull();
        _n3fsFsid.set(reply->fileSystemId);
        _n3fsFileid.set(reply->fileId);
        _n3fsCtime.setNull(); // not parsed
        _n3fsMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3fsAtime.setNull(); // not parsed
        NfsV3PduDescriptor::Fsstat fsstat;
        reply->getFsstat(&fsstat);
        _n3fsTbytes.set(fsstat.totalBytes);
        _n3fsFbytes.set(fsstat.freeBytes);
        _n3fsAbytes.set(fsstat.availFreeBytes);
        _n3fsTfiles.set(fsstat.totalFileSlots);
        _n3fsFfiles.set(fsstat.freeFileSlots);
        _n3fsAfiles.set(fsstat.availFreeFileSlots);
        _n3fsInvarsec.set(fsstat.invarSeconds);
    } else {
        _n3fsTypeId.setNull();
        _n3fsType.setNull();
        _n3fsMode.setNull();
        _n3fsNlink.setNull();
        _n3fsUid.setNull();
        _n3fsGid.setNull();
        _n3fsSize.setNull();
        _n3fsUsed.setNull();
        _n3fsSpecdata.setNull();
        _n3fsFsid.setNull();
        _n3fsFileid.setNull();
        _n3fsCtime.setNull();
        _n3fsMtime.setNull();
        _n3fsAtime.setNull();
        _n3fsTbytes.setNull();
        _n3fsFbytes.setNull();
        _n3fsAbytes.setNull();
        _n3fsTfiles.setNull();
        _n3fsFfiles.setNull();
        _n3fsAfiles.setNull();
        _n3fsInvarsec.setNull();
    }
}
