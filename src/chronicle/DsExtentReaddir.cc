// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentReaddir.h"
#include "DsWriter.h"

DsExtentReaddir::DsExtentReaddir() :
    _n3rdRecordId(_esN3rd, "record_id"),
    _n3rdDir(_esN3rd, "dir"),
    _n3rdCookie(_esN3rd, "cookie"),
    _n3rdCookieverf(_esN3rd, "cookieverf"),
    _n3rdDircount(_esN3rd, "dircount", Field::flag_nullable),
    _n3rdMaxcount(_esN3rd, "maxcount"),
    _n3rdStatus(_esN3rd, "status"),
    _n3rdDirTypeId(_esN3rd, "dir_type_id", Field::flag_nullable),
    _n3rdDirType(_esN3rd, "dir_type", Field::flag_nullable),
    _n3rdDirMode(_esN3rd, "dir_mode", Field::flag_nullable),
    _n3rdDirNlink(_esN3rd, "dir_nlink", Field::flag_nullable),
    _n3rdDirUid(_esN3rd, "dir_uid", Field::flag_nullable),
    _n3rdDirGid(_esN3rd, "dir_gid", Field::flag_nullable),
    _n3rdDirSize(_esN3rd, "dir_size", Field::flag_nullable),
    _n3rdDirUsed(_esN3rd, "dir_used", Field::flag_nullable),
    _n3rdDirSpecdata(_esN3rd, "dir_specdata", Field::flag_nullable),
    _n3rdDirFsid(_esN3rd, "dir_fsid", Field::flag_nullable),
    _n3rdDirFileid(_esN3rd, "dir_fileid", Field::flag_nullable),
    _n3rdDirCtime(_esN3rd, "dir_ctime", Field::flag_nullable),
    _n3rdDirMtime(_esN3rd, "dir_mtime", Field::flag_nullable),
    _n3rdDirAtime(_esN3rd, "dir_atime", Field::flag_nullable),
    _n3rdNewCookieverf(_esN3rd, "new_cookieverf", Field::flag_nullable),
    _n3rdEof(_esN3rd, "eof", Field::flag_nullable)
{
}

void
DsExtentReaddir::attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize)
{
    ExtentType::Ptr
        etN3rd(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_READDIR));
    _om = new OutputModule(*outfile, _esN3rd, etN3rd, extentSize);
    _writerEntries.attach(outfile, lib, extentSize);
}

void
DsExtentReaddir::detach()
{
    _writerEntries.detach();
    DsExtentWriter::detach();
}

void
DsExtentReaddir::write(NfsV3PduDescriptor *call, NfsV3PduDescriptor *reply)
{
    _om->newRecord();
    _n3rdRecordId.set(call->dsRecordId);
    _n3rdDir.set(call->fileHandle, call->fhLen);
    _n3rdCookie.set(0); // not parsed
    _n3rdCookieverf.set(""); // not parsed
    if (call->rpcProgramProcedure == NFS3PROC_READDIRPLUS) {
        _n3rdDircount.set(call->byteCount);
    } else {
        _n3rdDircount.setNull();
    }
    _n3rdMaxcount.set(call->maxByteCount);
    _n3rdStatus.set(reply->nfsStatus);
    // dir attr not parsed
    _n3rdDirTypeId.setNull();
    _n3rdDirType.setNull();
    _n3rdDirMode.setNull();
    _n3rdDirNlink.setNull();
    _n3rdDirUid.setNull();
    _n3rdDirGid.setNull();
    _n3rdDirSize.setNull();
    _n3rdDirUsed.setNull();
    _n3rdDirSpecdata.setNull();
    _n3rdDirFsid.setNull();
    _n3rdDirFileid.setNull();
    _n3rdDirCtime.setNull();
    _n3rdDirMtime.setNull();
    _n3rdDirAtime.setNull();
    if (NFS_OK == reply->nfsStatus) {
        _writerEntries.write(call, reply);
    }
}

DsExtentReaddir::DsExtentRDEntries::DsExtentRDEntries() :
    _n3deRecordId(_esN3de, "record_id"),
    _n3deFilename(_esN3de, "filename"),
    _n3deCookie(_esN3de, "cookie"),
    _n3deTypeId(_esN3de, "type_id", Field::flag_nullable),
    _n3deType(_esN3de, "type", Field::flag_nullable),
    _n3deMode(_esN3de, "mode", Field::flag_nullable),
    _n3deNlink(_esN3de, "nlink", Field::flag_nullable),
    _n3deUid(_esN3de, "uid", Field::flag_nullable),
    _n3deGid(_esN3de, "gid", Field::flag_nullable),
    _n3deSize(_esN3de, "size", Field::flag_nullable),
    _n3deUsed(_esN3de, "used", Field::flag_nullable),
    _n3deSpecdata(_esN3de, "specdata", Field::flag_nullable),
    _n3deFsid(_esN3de, "fsid", Field::flag_nullable),
    _n3deFileid(_esN3de, "fileid", Field::flag_nullable),
    _n3deCtime(_esN3de, "ctime", Field::flag_nullable),
    _n3deMtime(_esN3de, "mtime", Field::flag_nullable),
    _n3deAtime(_esN3de, "atime", Field::flag_nullable),
    _n3deFilehandle(_esN3de, "filehandle", Field::flag_nullable)
{
}

void
DsExtentReaddir::DsExtentRDEntries::attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize)
{
    ExtentType::Ptr
        etN3de(lib->registerTypePtr(EXTENT_CHRONICLE_NFS3_READDIR_ENTRIES));
    _om = new OutputModule(*outfile, _esN3de, etN3de, extentSize);
}

void
DsExtentReaddir::DsExtentRDEntries::write(NfsV3PduDescriptor *call,
                                          NfsV3PduDescriptor *reply)
{
    bool end = false;
    while (true) {
        char filename[NFS_MAXNAMLEN+1];
        if (call->rpcProgramProcedure == NFS3PROC_READDIRPLUS) {
            if (reply->getReaddirplusEntry(
                    reinterpret_cast<unsigned char *>(filename), end) 
                && !end) {
                _om->newRecord();
                _n3deTypeId.set(reply->type);
                _n3deType.set(DsWriter::typeIdToName(reply->type));
                _n3deMode.set(0xFFF & reply->fileMode);
                _n3deNlink.setNull(); // not parsed
                _n3deUid.set(reply->fileUid);
                _n3deGid.set(reply->fileGid);
                _n3deSize.set(reply->fileSizeBytes);
                _n3deUsed.set(reply->fileUsedBytes);
                _n3deSpecdata.setNull();
                _n3deFsid.set(reply->fileSystemId);
                _n3deFileid.set(reply->fileId);
                _n3deCtime.setNull(); // not parsed
                _n3deMtime.set(DsWriter::nfstimeToNs(reply->fileModTime));
                _n3deAtime.setNull(); // not parsed
                _n3deFilehandle.set(reply->fileHandle, reply->fhLen);
            } else {
                end = true;
                break;
            }
        } else {
            if (reply->getReaddirEntry(
                    reinterpret_cast<unsigned char *>(filename), end) &&
                !end) {
                _om->newRecord();
                _n3deTypeId.setNull();
                _n3deType.setNull();
                _n3deMode.setNull();
                _n3deNlink.setNull();
                _n3deUid.setNull();
                _n3deGid.setNull();
                _n3deSize.setNull();
                _n3deUsed.setNull();
                _n3deSpecdata.setNull();
                _n3deFsid.setNull();
                _n3deFileid.set(reply->fileId);
                _n3deCtime.setNull();
                _n3deMtime.setNull();
                _n3deAtime.setNull();
                _n3deFilehandle.setNull();
            } else {
                end = true;
                break;
            }
        }
        _n3deRecordId.set(call->dsRecordId);
        _n3deFilename.set(filename, strnlen(filename, NFS_MAXNAMLEN+1));
    }
}
