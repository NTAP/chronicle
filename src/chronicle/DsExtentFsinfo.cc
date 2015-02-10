// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentFsinfo.h"
#include "DsWriter.h"

DsExtentFsinfo::DsExtentFsinfo() :
    _n3fiRecordId(_esN3fi, "record_id"),
    _n3fiFilehandle(_esN3fi, "filehandle"),
    _n3fiStatus(_esN3fi, "status"),
    _n3fiTypeId(_esN3fi, "type_id", Field::flag_nullable),
    _n3fiType(_esN3fi, "type", Field::flag_nullable),
    _n3fiMode(_esN3fi, "mode", Field::flag_nullable),
    _n3fiNlink(_esN3fi, "nlink", Field::flag_nullable),
    _n3fiUid(_esN3fi, "uid", Field::flag_nullable),
    _n3fiGid(_esN3fi, "gid", Field::flag_nullable),
    _n3fiSize(_esN3fi, "size", Field::flag_nullable),
    _n3fiUsed(_esN3fi, "used", Field::flag_nullable),
    _n3fiSpecdata(_esN3fi, "specdata", Field::flag_nullable),
    _n3fiFsid(_esN3fi, "fsid", Field::flag_nullable),
    _n3fiFileid(_esN3fi, "fileid", Field::flag_nullable),
    _n3fiCtime(_esN3fi, "ctime", Field::flag_nullable),
    _n3fiMtime(_esN3fi, "mtime", Field::flag_nullable),
    _n3fiAtime(_esN3fi, "atime", Field::flag_nullable),
    _n3fiRtmax(_esN3fi, "rtmax", Field::flag_nullable),
    _n3fiRtpref(_esN3fi, "rtpref", Field::flag_nullable),
    _n3fiRtmult(_esN3fi, "rtmult", Field::flag_nullable),
    _n3fiWtmax(_esN3fi, "wtmax", Field::flag_nullable),
    _n3fiWtpref(_esN3fi, "wtpref", Field::flag_nullable),
    _n3fiWtmult(_esN3fi, "wtmult", Field::flag_nullable),
    _n3fiDtpref(_esN3fi, "dtpref", Field::flag_nullable),
    _n3fiMaxfilesize(_esN3fi, "maxfilesize", Field::flag_nullable),
    _n3fiTimeDelta(_esN3fi, "time_delta", Field::flag_nullable),
    _n3fiProperties(_esN3fi, "properties", Field::flag_nullable)
{
}

void
DsExtentFsinfo::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3fi(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_FSINFO));
    _om = new OutputModule(*outfile, _esN3fi, etN3fi, extentSize);
}

void
DsExtentFsinfo::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3fiRecordId.set(call->dsRecordId);
    _n3fiFilehandle.set(call->fileHandle, call->fhLen);
    _n3fiStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3fiTypeId.set(reply->type);
        _n3fiType.set(DsWriter::typeIdToName(reply->type));
        _n3fiMode.set(0xFFF & reply->fileMode);
        _n3fiNlink.setNull(); // not parsed
        _n3fiUid.set(reply->fileUid);
        _n3fiGid.set(reply->fileGid);
        _n3fiSize.set(reply->fileSizeBytes);
        _n3fiUsed.set(reply->fileUsedBytes);
        _n3fiSpecdata.setNull();
        _n3fiFsid.set(reply->fileSystemId);
        _n3fiFileid.set(reply->fileId);
        _n3fiCtime.setNull(); // not parsed
        _n3fiMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3fiAtime.setNull(); // not parsed
        NfsV3PduDescriptor::Fsinfo fsinfo;
        reply->getFsinfo(&fsinfo);
        _n3fiRtmax.set(fsinfo.rtmax);
        _n3fiRtpref.set(fsinfo.rtpref);
        _n3fiRtmult.set(fsinfo.rtmult);
        _n3fiWtmax.set(fsinfo.wtmax);
        _n3fiWtpref.set(fsinfo.wtpref);
        _n3fiWtmult.set(fsinfo.wtmult);
        _n3fiDtpref.set(fsinfo.dtpref);
        _n3fiMaxfilesize.set(fsinfo.maxFileSize);
        _n3fiTimeDelta.set(DsWriter::nfstimeToNs(fsinfo.timeDelta));
        _n3fiProperties.set(fsinfo.properties);
    } else {
        _n3fiTypeId.setNull();
        _n3fiType.setNull();
        _n3fiMode.setNull();
        _n3fiNlink.setNull();
        _n3fiUid.setNull();
        _n3fiGid.setNull();
        _n3fiSize.setNull();
        _n3fiUsed.setNull();
        _n3fiSpecdata.setNull();
        _n3fiFsid.setNull();
        _n3fiFileid.setNull();
        _n3fiCtime.setNull();
        _n3fiMtime.setNull();
        _n3fiAtime.setNull();
        _n3fiRtmax.setNull();
        _n3fiRtpref.setNull();
        _n3fiRtmult.setNull();
        _n3fiWtmax.setNull();
        _n3fiWtpref.setNull();
        _n3fiWtmult.setNull();
        _n3fiDtpref.setNull();
        _n3fiMaxfilesize.setNull();
        _n3fiTimeDelta.setNull();
        _n3fiProperties.setNull();
    }
}
