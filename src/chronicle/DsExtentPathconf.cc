// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentPathconf.h"
#include "DsWriter.h"

DsExtentPathconf::DsExtentPathconf() :
    _n3pcRecordId(_esN3pc, "record_id"),
    _n3pcFilehandle(_esN3pc, "filehandle"),
    _n3pcStatus(_esN3pc, "status"),
    _n3pcTypeId(_esN3pc, "type_id", Field::flag_nullable),
    _n3pcType(_esN3pc, "type", Field::flag_nullable),
    _n3pcMode(_esN3pc, "mode", Field::flag_nullable),
    _n3pcNlink(_esN3pc, "nlink", Field::flag_nullable),
    _n3pcUid(_esN3pc, "uid", Field::flag_nullable),
    _n3pcGid(_esN3pc, "gid", Field::flag_nullable),
    _n3pcSize(_esN3pc, "size", Field::flag_nullable),
    _n3pcUsed(_esN3pc, "used", Field::flag_nullable),
    _n3pcSpecdata(_esN3pc, "specdata", Field::flag_nullable),
    _n3pcFsid(_esN3pc, "fsid", Field::flag_nullable),
    _n3pcFileid(_esN3pc, "fileid", Field::flag_nullable),
    _n3pcCtime(_esN3pc, "ctime", Field::flag_nullable),
    _n3pcMtime(_esN3pc, "mtime", Field::flag_nullable),
    _n3pcAtime(_esN3pc, "atime", Field::flag_nullable),
    _n3pcLinkmax(_esN3pc, "linkmax", Field::flag_nullable),
    _n3pcNamemax(_esN3pc, "namemax", Field::flag_nullable),
    _n3pcNoTrunc(_esN3pc, "no_trunc", Field::flag_nullable),
    _n3pcChownRestricted(_esN3pc, "chown_restricted", Field::flag_nullable),
    _n3pcCaseInsensitive(_esN3pc, "case_insensitive", Field::flag_nullable),
    _n3pcCasePreserving(_esN3pc, "case_preserving", Field::flag_nullable)
{
}

void
DsExtentPathconf::attach(DataSeriesSink *outfile,
                       ExtentTypeLibrary *lib,
                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3pc(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_PATHCONF));
    _om = new OutputModule(*outfile, _esN3pc, etN3pc, extentSize);
}

void
DsExtentPathconf::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3pcRecordId.set(call->dsRecordId);
    _n3pcFilehandle.set(call->fileHandle, call->fhLen);
    _n3pcStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        _n3pcTypeId.set(reply->type);
        _n3pcType.set(DsWriter::typeIdToName(reply->type));
        _n3pcMode.set(0xFFF & reply->fileMode);
        _n3pcNlink.setNull(); // not parsed
        _n3pcUid.set(reply->fileUid);
        _n3pcGid.set(reply->fileGid);
        _n3pcSize.set(reply->fileSizeBytes);
        _n3pcUsed.set(reply->fileUsedBytes);
        _n3pcSpecdata.setNull();
        _n3pcFsid.set(reply->fileSystemId);
        _n3pcFileid.set(reply->fileId);
        _n3pcCtime.setNull(); // not parsed
        _n3pcMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3pcAtime.setNull(); // not parsed
        NfsV3PduDescriptor::Pathconf pathconf;
        reply->getPathconf(&pathconf);
        _n3pcLinkmax.set(pathconf.linkMax);
        _n3pcNamemax.set(pathconf.nameMax);
        _n3pcNoTrunc.set(pathconf.noTrunc);
        _n3pcChownRestricted.set(pathconf.chownRestrict);
        _n3pcCaseInsensitive.set(pathconf.caseInsensitive);
        _n3pcCasePreserving.set(pathconf.casePreserving);
    } else {
        _n3pcTypeId.setNull();
        _n3pcType.setNull();
        _n3pcMode.setNull();
        _n3pcNlink.setNull();
        _n3pcUid.setNull();
        _n3pcGid.setNull();
        _n3pcSize.setNull();
        _n3pcUsed.setNull();
        _n3pcSpecdata.setNull();
        _n3pcFsid.setNull();
        _n3pcFileid.setNull();
        _n3pcCtime.setNull();
        _n3pcMtime.setNull();
        _n3pcAtime.setNull();
        _n3pcLinkmax.setNull();
        _n3pcNamemax.setNull();
        _n3pcNoTrunc.setNull();
        _n3pcChownRestricted.setNull();
        _n3pcCaseInsensitive.setNull();
        _n3pcCasePreserving.setNull();
    }
}
