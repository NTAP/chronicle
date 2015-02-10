// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentGetattr.h"
#include "DsWriter.h"

DsExtentGetattr::DsExtentGetattr() :
    _n3gaRecordId(_esN3ga, "record_id"),
    _n3gaFilehandle(_esN3ga, "filehandle"),
    _n3gaStatus(_esN3ga, "status"),
    _n3gaTypeId(_esN3ga, "type_id", Field::flag_nullable),
    _n3gaType(_esN3ga, "type", Field::flag_nullable),
    _n3gaMode(_esN3ga, "mode", Field::flag_nullable),
    _n3gaNlink(_esN3ga, "nlink", Field::flag_nullable),
    _n3gaUid(_esN3ga, "uid", Field::flag_nullable),
    _n3gaGid(_esN3ga, "gid", Field::flag_nullable),
    _n3gaSize(_esN3ga, "size", Field::flag_nullable),
    _n3gaUsed(_esN3ga, "used", Field::flag_nullable),
    _n3gaSpecdata(_esN3ga, "specdata", Field::flag_nullable),
    _n3gaFsid(_esN3ga, "fsid", Field::flag_nullable),
    _n3gaFileid(_esN3ga, "fileid", Field::flag_nullable),
    _n3gaCtime(_esN3ga, "ctime", Field::flag_nullable),
    _n3gaMtime(_esN3ga, "mtime", Field::flag_nullable),
    _n3gaAtime(_esN3ga, "atime", Field::flag_nullable)
{
}

void
DsExtentGetattr::attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize)
{
    ExtentType::Ptr
        etN3ga(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_GETATTR));
    _om = new OutputModule(*outfile, _esN3ga, etN3ga, extentSize);
}

void
DsExtentGetattr::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3gaRecordId.set(call->dsRecordId);
    _n3gaFilehandle.set(call->fileHandle, call->fhLen);
    _n3gaStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3gaTypeId.set(reply->type);
        _n3gaType.set(DsWriter::typeIdToName(reply->type));
        _n3gaMode.set(0xFFF & reply->fileMode);
        _n3gaNlink.setNull(); // not parsed
        _n3gaUid.set(reply->fileUid);
        _n3gaGid.set(reply->fileGid);
        _n3gaSize.set(reply->fileSizeBytes);
        _n3gaUsed.set(reply->fileUsedBytes);
        _n3gaSpecdata.setNull();
        _n3gaFsid.set(reply->fileSystemId);
        _n3gaFileid.set(reply->fileId);
        _n3gaCtime.setNull(); // not parsed
        _n3gaMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3gaAtime.setNull(); // not parsed
    } else {
        _n3gaTypeId.setNull();
        _n3gaType.setNull();
        _n3gaMode.setNull();
        _n3gaNlink.setNull();
        _n3gaUid.setNull();
        _n3gaGid.setNull();
        _n3gaSize.setNull();
        _n3gaUsed.setNull();
        _n3gaSpecdata.setNull();
        _n3gaFsid.setNull();
        _n3gaFileid.setNull();
        _n3gaCtime.setNull();
        _n3gaMtime.setNull();
        _n3gaAtime.setNull();
    }
}
