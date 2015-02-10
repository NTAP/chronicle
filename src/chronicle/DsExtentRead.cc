// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentRead.h"
#include "DsWriter.h"

DsExtentRead::DsExtentRead() :
    _n3rwRecordId(_esN3rw, "record_id"),
    _n3rwIsRead(_esN3rw, "is_read"),
    _n3rwFile(_esN3rw, "file"),
    _n3rwOffset(_esN3rw, "offset"),
    _n3rwCount(_esN3rw, "count"),
    _n3rwStable(_esN3rw, "stable", Field::flag_nullable),
    _n3rwStatus(_esN3rw, "status"),
    _n3rwPreopSize(_esN3rw, "preop_size", Field::flag_nullable),
    _n3rwPreopCtime(_esN3rw, "preop_ctime", Field::flag_nullable),
    _n3rwPreopMtime(_esN3rw, "preop_mtime", Field::flag_nullable),
    _n3rwPostopTypeId(_esN3rw, "postop_type_id", Field::flag_nullable),
    _n3rwPostopType(_esN3rw, "postop_type", Field::flag_nullable),
    _n3rwPostopMode(_esN3rw, "postop_mode", Field::flag_nullable),
    _n3rwPostopNlink(_esN3rw, "postop_nlink", Field::flag_nullable),
    _n3rwPostopUid(_esN3rw, "postop_uid", Field::flag_nullable),
    _n3rwPostopGid(_esN3rw, "postop_gid", Field::flag_nullable),
    _n3rwPostopSize(_esN3rw, "postop_size", Field::flag_nullable),
    _n3rwPostopUsed(_esN3rw, "postop_used", Field::flag_nullable),
    _n3rwPostopSpecdata(_esN3rw, "postop_specdata", Field::flag_nullable),
    _n3rwPostopFsid(_esN3rw, "postop_fsid", Field::flag_nullable),
    _n3rwPostopFileid(_esN3rw, "postop_fileid", Field::flag_nullable),
    _n3rwPostopCtime(_esN3rw, "postop_ctime", Field::flag_nullable),
    _n3rwPostopMtime(_esN3rw, "postop_mtime", Field::flag_nullable),
    _n3rwPostopAtime(_esN3rw, "postop_atime", Field::flag_nullable),
    _n3rwCountActual(_esN3rw, "count_actual", Field::flag_nullable),
    _n3rwStableRes(_esN3rw, "stable_res", Field::flag_nullable),
    _n3rwEof(_esN3rw, "eof", Field::flag_nullable),
    _n3rwWriteverf(_esN3rw, "writeverf", Field::flag_nullable)
{
}

void
DsExtentRead::attach(DataSeriesSink *outfile,
                     ExtentTypeLibrary *lib,
                     unsigned extentSize)
{
    ExtentType::Ptr
        etN3rw(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_READWRITE));
    _om = new OutputModule(*outfile, _esN3rw, etN3rw, extentSize);
    if (dsEnableIpChecksum) {
        _writerChecksum.attach(outfile, lib, extentSize);
    }
}

void
DsExtentRead::detach()
{
    if (dsEnableIpChecksum) {
        _writerChecksum.detach();
    }
    DsExtentWriter::detach();
}

void
DsExtentRead::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();

    bool isRead = call->rpcProgramProcedure == NFS3PROC_READ;
    _n3rwRecordId.set(call->dsRecordId);
    _n3rwIsRead.set(isRead);
    _n3rwFile.set(call->fileHandle, call->fhLen);
    _n3rwOffset.set(call->fileOffset);
    _n3rwCount.set(call->byteCount);
    if (isRead) {
        _n3rwStable.setNull();
    } else {
        _n3rwStable.set(call->writeStable);
    }
    _n3rwStatus.set(reply->nfsStatus);
    if (NFS_OK == reply->nfsStatus) {
        if (dsEnableIpChecksum) {
            _writerChecksum.write(call, reply);
        }
        _n3rwPreopSize.setNull(); // not parsed
        _n3rwPreopCtime.setNull(); // not parsed
        _n3rwPreopMtime.setNull(); // not parsed
        _n3rwPostopTypeId.set(reply->type);
        _n3rwPostopType.set(DsWriter::typeIdToName(reply->type));
        _n3rwPostopMode.set(0xFFF & reply->fileMode);
        _n3rwPostopNlink.setNull(); // not parsed
        _n3rwPostopUid.set(reply->fileUid);
        _n3rwPostopGid.set(reply->fileGid);
        _n3rwPostopSize.set(reply->fileSizeBytes);
        _n3rwPostopUsed.set(reply->fileUsedBytes);
        _n3rwPostopSpecdata.setNull(); // not parsed
        _n3rwPostopFsid.set(reply->fileSystemId);
        _n3rwPostopFileid.set(reply->fileId);
        _n3rwPostopCtime.setNull(); // not parsed
        _n3rwPostopMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
        _n3rwPostopAtime.setNull(); // not parsed
        _n3rwCountActual.set(reply->byteCount);
        if (isRead) {
            _n3rwStableRes.setNull();
        } else {
            _n3rwStableRes.set(reply->writeStable);
        }
        _n3rwEof.setNull(); // not parsed
        _n3rwWriteverf.setNull(); // not parsed
    } else {
        _n3rwPreopSize.setNull();
        _n3rwPreopCtime.setNull();
        _n3rwPreopMtime.setNull();
        _n3rwPostopTypeId.setNull();
        _n3rwPostopType.setNull();
        _n3rwPostopMode.setNull();
        _n3rwPostopNlink.setNull();
        _n3rwPostopUid.setNull();
        _n3rwPostopGid.setNull();
        _n3rwPostopSize.setNull();
        _n3rwPostopUsed.setNull();
        _n3rwPostopSpecdata.setNull();
        _n3rwPostopFsid.setNull();
        _n3rwPostopFileid.setNull();
        _n3rwPostopCtime.setNull();
        _n3rwPostopMtime.setNull();
        _n3rwPostopAtime.setNull();
        _n3rwCountActual.setNull();
        _n3rwStableRes.setNull();
        _n3rwEof.setNull();
        _n3rwWriteverf.setNull();
    }
}


DsExtentRead::DsExtentChecksum::DsExtentChecksum() :
    _n3xsRecordId(_esN3xs, "record_id"),
    _n3xsOffset(_esN3xs, "offset"),
    _n3xsChecksum(_esN3xs, "checksum")
{
}

void
DsExtentRead::DsExtentChecksum::attach(DataSeriesSink *outfile,
                                       ExtentTypeLibrary *lib,
                                       unsigned extentSize)
{
    ExtentType::Ptr
        etN3xs(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_RWCHECKSUM));
    _om = new OutputModule(*outfile, _esN3xs, etN3xs, extentSize);
}

void
DsExtentRead::DsExtentChecksum::write(NfsV3PduDescriptor *call,
                                      NfsV3PduDescriptor *reply)
{
    // Figure out which descriptor should have the checksums in it
    NfsV3PduDescriptor *xsumDesc =
        (call->rpcProgramProcedure == NFS3PROC_READ) ? reply : call;
    while (!xsumDesc->dataChecksums.empty()) {
        NfsV3PduDescriptor::ChecksumRecord *r =
            &xsumDesc->dataChecksums.front();
        _om->newRecord();
        _n3xsRecordId.set(call->dsRecordId);
        _n3xsOffset.set(r->offset);
        _n3xsChecksum.set(r->checksum);
        xsumDesc->dataChecksums.pop_front();
    }
}
