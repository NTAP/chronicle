// -*- mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"

/**
 * @file
 * This file defines the DataSeries extent types that are used by Chronicle.
 * 
 * Field definitions:
 * - name: text name of the field
 * - type: bool | byte | double | fixedwidth | int32 | int64 | variable32
 * - opt_nullable: yes  Allow the field to be marked as NULL in a record
 * - print_divisor: <number> {<numeric fields>} divide the value by this number before printing
 * - print_format: printf-style format specifier for displaying the field
 * - print_style: csv | hex | maybehex | text {variable32} how the field should be displayed
 * - print_[true|false]: <text> {bool} text values to print instead of true/false for boolean fields
 * - pack_relative: <fieldname> {<numeric fields>} use deltas to compress this field relative to the named one
 * - pack_scale: <number> {double} The value will be divided by <number> and rounded to an int before storing
 * - pack_unique: yes {variable32} use embedded table-lookup to eliminate duplicate values
 * - comment: text comment that describes the semantics of the field (for humans)
 * https://github.com/dataseries/DataSeries/wiki/Defining-types
 */


/*
 * Fields that will be used in several different extent types.
 * We define them once here to ensure uniformity
 */
#define FIELD_RECORD_ID_CUSTOM(fieldname) \
    "  <field name=\"" fieldname "\" type=\"int64\" comment=\"for correlating with the records in other extent types\" pack_relative=\"" fieldname "\" />\n"
#define FIELD_RECORD_ID FIELD_RECORD_ID_CUSTOM("record_id")
#define FIELD_TIME_R(fieldname, comment, relativefield)                    \
    "  <field name=\"" fieldname "\" type=\"int64\" comment=\"" comment "ns since UNIX epoch\" pack_relative=\"" relativefield "\" print_format=\"%llu\"/>\n"
#define FIELD_TIME_N(fieldname, comment)                                        \
    "  <field name=\"" fieldname "\" type=\"int64\" opt_nullable=\"yes\" comment=\"" comment "ns since UNIX epoch\" print_format=\"%llu\"/>\n"
#define FIELD_TIME(fieldname, comment) FIELD_TIME_R(fieldname, comment, fieldname)
#define FIELD_NFSFH(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"hex\" />\n"
#define FIELD_NFSFH_N(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" opt_nullable=\"yes\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"hex\" />\n"
#define FIELD_NFSFNAME(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"text\" />\n"
#define FIELD_NFSPATH(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"text\" />\n"
#define FIELD_NFSPATH_N(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" opt_nullable=\"yes\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"text\" />\n"
#define FIELD_NFSSTAT \
    "  <field name=\"status\" type=\"int32\" comment=\"NFS operation result\" />\n"
#define FIELD_VERF(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"hex\" />\n"
#define FIELD_VERF_N(fieldname, comment) \
    "  <field name=\"" fieldname "\" type=\"variable32\" opt_nullable=\"yes\" comment=\"" comment "\" pack_unique=\"yes\" print_style=\"hex\" />\n"
#define GROUP_RECORD_ID \
    FIELD_RECORD_ID_CUSTOM("record_id_call") \
    FIELD_RECORD_ID_CUSTOM("record_id_reply")
#define GROUP_FATTR(prefix)                                             \
    "  <field name=\"" prefix "type_id\" type=\"byte\" opt_nullable=\"yes\" comment=\"file type (raw number)\" />\n" \
    "  <field name=\"" prefix "type\" type=\"variable32\" opt_nullable=\"yes\" comment=\"file type as text\" />\n" \
    "  <field name=\"" prefix "mode\" type=\"int32\" opt_nullable=\"yes\" comment=\"file permisson bits; only includes bits 0xFFF\" />\n" \
    "  <field name=\"" prefix "nlink\" type=\"int32\" opt_nullable=\"yes\" comment=\"hard link count\" />\n" \
    "  <field name=\"" prefix "uid\" type=\"int32\" opt_nullable=\"yes\" comment=\"owner's user ID\" />\n" \
    "  <field name=\"" prefix "gid\" type=\"int32\" opt_nullable=\"yes\" comment=\"owner's group ID\" />\n" \
    "  <field name=\"" prefix "size\" type=\"int64\" opt_nullable=\"yes\" comment=\"size in bytes\" />\n" \
    "  <field name=\"" prefix "used\" type=\"int64\" opt_nullable=\"yes\" comment=\"size in bytes consumed on disk\" />\n" \
    "  <field name=\"" prefix "specdata\" type=\"int64\" opt_nullable=\"yes\" comment=\"device major/minor number (top 32-bits is major, bottom 32-bits is minor)\" />\n" \
    "  <field name=\"" prefix "fsid\" type=\"int64\" opt_nullable=\"yes\" comment=\"file system identifier\" />\n" \
    "  <field name=\"" prefix "fileid\" type=\"int64\" opt_nullable=\"yes\" comment=\"inode number\" />\n" \
    FIELD_TIME_N(prefix "ctime", "file's last attribute change time in ") \
    FIELD_TIME_N(prefix "mtime", "file's last modification time in ") \
    FIELD_TIME_N(prefix "atime", "file's last access time in ")
#define GROUP_SATTR(prefix)                                             \
    "  <field name=\"" prefix "mode\" type=\"int32\" opt_nullable=\"yes\" comment=\"file permisson bits; only includes bits 0xFFF\" />\n" \
    "  <field name=\"" prefix "uid\" type=\"int32\" opt_nullable=\"yes\" comment=\"owner's user ID\" />\n" \
    "  <field name=\"" prefix "gid\" type=\"int32\" opt_nullable=\"yes\" comment=\"owner's group ID\" />\n" \
    "  <field name=\"" prefix "size\" type=\"int64\" opt_nullable=\"yes\" comment=\"size in bytes\" />\n" \
    "  <field name=\"" prefix "mtime\" type=\"int64\" opt_nullable=\"yes\" comment=\"file's last modification time in ns since UNIX epoch\" />\n" \
    "  <field name=\"" prefix "atime\" type=\"int64\" opt_nullable=\"yes\" comment=\"file's last access time in ns since UNIX epoch\" />\n"
#define GROUP_WCC_ATTR(prefix)                                          \
    "  <field name=\"" prefix "size\" type=\"int64\" opt_nullable=\"yes\" comment=\"size in bytes\" />\n" \
    FIELD_TIME_N(prefix "ctime", "file's last attribute change time in ") \
    FIELD_TIME_N(prefix "mtime", "file's last modification time in ")
#define GROUP_WCC_DATA(prefix)                  \
    GROUP_WCC_ATTR(prefix "preop_")              \
    GROUP_FATTR(prefix "postop_")
#define GROUP_DIROPARGS(prefix)                 \
    FIELD_NFSFH(prefix "dirfh", "filehandle of a directory") \
    FIELD_NFSFNAME(prefix "filename", "filename within the directory")


const char *EXTENT_CHRONICLE_IP = 
    "<ExtentType name=\"trace::net::ip\" namespace=\"atg.netapp.com\" version=\"1.1\" >\n"
    FIELD_TIME("packet_at", "time in ")
    "  <field name=\"interface\" type=\"variable32\" pack_unique=\"yes\" comment=\"network interface that received this packet\" />\n"
    "  <field name=\"ring_num\" type=\"byte\" comment=\"network interface ring number that received this packet\" />\n"
    "  <field name=\"source\" type=\"int32\" comment=\"32-bit packed IPv4 address\" print_format=\"%08x\" />\n"
    "  <field name=\"source_port\" type=\"int32\" />\n"
    "  <field name=\"source_mac\" type=\"variable32\" pack_unique=\"yes\" print_style=\"hex\" comment=\"source Ethernet MAC address\" />\n"
    "  <field name=\"dest\" type=\"int32\" comment=\"32-bit packed IPv4 address\" print_format=\"%08x\" />\n"
    "  <field name=\"dest_port\" type=\"int32\" />\n"
    "  <field name=\"dest_mac\" type=\"variable32\" pack_unique=\"yes\" print_style=\"hex\" comment=\"destination Ethernet MAC address\" />\n"
    "  <field name=\"wire_length\" type=\"int32\" />\n"
    "  <field name=\"protocol\" type=\"byte\" comment=\"IP protocol number\" />\n"
    "  <field name=\"is_fragment\" type=\"bool\" />\n"
    "  <field name=\"tcp_seqnum\" type=\"int32\" opt_nullable=\"yes\" print_format=\"%u\" />\n"
    "  <field name=\"tcp_payload_length\" type=\"int32\" opt_nullable=\"yes\" print_format=\"%u\" comment=\"number of data bytes in the TCP payload\" />\n"
    "  <field name=\"is_good_pdu\" type=\"bool\" comment=\"true if this packet is a part of a successfully parsed PDU\" />\n"
    FIELD_RECORD_ID
    "</ExtentType>\n";

/**
 * The RPC extent.
 * There is one of these records for each SUNRPC call & reply that is captured by Chronicle.
 */
const char *EXTENT_CHRONICLE_RPCCOMMON =
    "<ExtentType name=\"trace::rpc\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    FIELD_TIME("packet_at", "time in ")
    "  <field name=\"source\" type=\"int32\" comment=\"32-bit packed IPv4 address\" print_format=\"%08x\" />\n"
    "  <field name=\"source_port\" type=\"int32\" />\n"
    "  <field name=\"dest\" type=\"int32\" comment=\"32-bit packed IPv4 address\" print_format=\"%08x\" />\n"
    "  <field name=\"dest_port\" type=\"int32\" />\n"
    "  <field name=\"is_udp\" type=\"bool\" print_true=\"UDP\" print_false=\"TCP\" />\n"
    "  <field name=\"is_request\" type=\"bool\" print_true=\"request\" print_false=\"response\" />\n"
    "  <field name=\"rpc_program\" type=\"int32\" comment=\"sunrpc program number\" />\n"
    "  <field name=\"program_version\" type=\"int32\" comment=\"sunrpc program version number\" />\n"
    "  <field name=\"program_proc\" type=\"int32\" comment=\"sunrpc procedure number\" />\n"
    "  <field name=\"operation\" type=\"variable32\" pack_unique=\"yes\" comment=\"text name for the rpc program/version/procedure\"/>\n"
    "  <field name=\"transaction_id\" type=\"int32\" print_format=\"%08x\" comment=\"sunrpc transaction id\" />\n"
    "  <field name=\"header_offset\" type=\"int32\" comment=\"offset in bytes of the rpc header in the 1st IP packet\" />\n"
    "  <field name=\"payload_length\" type=\"int32\" comment=\"length in bytes of the rpc call/reply\"/>\n"
    FIELD_RECORD_ID
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_GETATTR =
    "<ExtentType name=\"trace::rpc::nfs3::getattr\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "the filehandle for which attributes are being requested")
    FIELD_NFSSTAT
    GROUP_FATTR("")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_SETATTR =
    "<ExtentType name=\"trace::rpc::nfs3::setattr\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "the filehandle for which attributes are being set")
    GROUP_SATTR("set_")
    FIELD_TIME_N("ctime_guard", "ctime for guarding sattr operation in ")
    FIELD_NFSSTAT
    GROUP_WCC_DATA("")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_LOOKUP =
    "<ExtentType name=\"trace::rpc::nfs3::lookup\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    GROUP_DIROPARGS("")
    FIELD_NFSSTAT
    FIELD_NFSFH_N("filehandle", "filehandle of the named file")
    GROUP_FATTR("obj_")
    GROUP_FATTR("parent_")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_ACCESS =
    "<ExtentType name=\"trace::rpc::nfs3::access\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "filehandle on which to check permission")
    "  <field name=\"access\" type=\"int32\" comment=\"access rights to check\" />\n"
    FIELD_NFSSTAT
    GROUP_FATTR("postop_")
    "  <field name=\"access_res\" type=\"int32\" opt_nullable=\"yes\" comment=\"allowed access\" />\n"
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_READLINK =
    "<ExtentType name=\"trace::rpc::nfs3::readlink\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "symlink to be read")
    FIELD_NFSSTAT
    GROUP_FATTR("postop_")
    FIELD_NFSPATH_N("target", "target of the symlink")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_READWRITE =
    "<ExtentType name=\"trace::rpc::nfs3::readwrite\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    "  <field name=\"is_read\" type=\"bool\" print_true=\"read\" print_false=\"write\" comment=\"true for read, false for write\" />\n"
    FIELD_NFSFH("file", "file to be read/written")
    "  <field name=\"offset\" type=\"int64\" comment=\"file offset in bytes\" />\n"
    "  <field name=\"count\" type=\"int32\" comment=\"number of bytes to read/write\" />\n"
    "  <field name=\"stable\" type=\"byte\" opt_nullable=\"yes\" comment=\"stable flag (writes)\" />\n"
    FIELD_NFSSTAT
    GROUP_WCC_DATA("")
    "  <field name=\"count_actual\" type=\"int32\" opt_nullable=\"yes\" comment=\"actual number of bytes\" />\n"
    "  <field name=\"stable_res\" type=\"byte\" opt_nullable=\"yes\" comment=\"actual stable flag (writes)\" />\n"
    "  <field name=\"eof\" type=\"bool\" opt_nullable=\"yes\" comment=\"was EOF encountered (reads)\" />\n"
    FIELD_VERF_N("writeverf", "write verifier (writes)")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_RWCHECKSUM =
    "<ExtentType name=\"trace::rpc::nfs3::rwchecksum\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    "  <field name=\"offset\" type=\"int64\" comment=\"file offset in bytes\" />\n"
    "  <field name=\"checksum\" type=\"int64\" print_format=\"%016llx\" comment=\"hash of the 512B block starting at offset\" />\n"
    "</ExtentType>\n";

// create, mkdir, symlink, mknod
const char *EXTENT_CHRONICLE_NFS3_CREATE =
    "<ExtentType name=\"trace::rpc::nfs3::create\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    GROUP_DIROPARGS("")
    "  <field name=\"cmode\" type=\"byte\" opt_nullable=\"yes\" comment=\"mode for the new file (create)\" />\n"
    "  <field name=\"type_id\" type=\"byte\" comment=\"file type (raw number)\" />\n"
    "  <field name=\"type\" type=\"variable32\" comment=\"file type as text\" />\n"
    GROUP_SATTR("")
    FIELD_VERF_N("excl_createverf", "create verifier (create)")
    FIELD_NFSPATH_N("target", "target of the symlink (symlink)")
    "  <field name=\"specdata\" type=\"int64\" opt_nullable=\"yes\" comment=\"device major/minor number (top 32-bits is major, bottom 32-bits is minor) (mknod)\" />\n" \
    FIELD_NFSSTAT
    FIELD_NFSFH_N("filehandle", "filehandle of the new object")
    GROUP_FATTR("obj_")
    GROUP_WCC_DATA("parent_")
    "</ExtentType>\n";

// remove, rmdir
const char *EXTENT_CHRONICLE_NFS3_REMOVE =
    "<ExtentType name=\"trace::rpc::nfs3::remove\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    GROUP_DIROPARGS("")
    FIELD_NFSSTAT
    GROUP_WCC_DATA("parent_")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_RENAME =
    "<ExtentType name=\"trace::rpc::nfs3::rename\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    GROUP_DIROPARGS("from_")
    GROUP_DIROPARGS("to_")
    FIELD_NFSSTAT
    GROUP_WCC_DATA("fromdir_")
    GROUP_WCC_DATA("todir_")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_LINK =
    "<ExtentType name=\"trace::rpc::nfs3::link\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "object to link")
    GROUP_DIROPARGS("")
    FIELD_NFSSTAT
    GROUP_FATTR("obj_")
    GROUP_WCC_DATA("parent_")
    "</ExtentType>\n";

// readdir & readdirplus
const char *EXTENT_CHRONICLE_NFS3_READDIR =
    "<ExtentType name=\"trace::rpc::nfs3::readdir\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("dir", "directory to read")
    "  <field name=\"cookie\" type=\"int64\" comment=\"readdir cookie\" />\n"
    FIELD_VERF("cookieverf", "cookie verifier")
    "  <field name=\"dircount\" type=\"int32\" opt_nullable=\"yes\" comment=\"max bytes of directory info to read (readdirplus)\" />\n"
    "  <field name=\"maxcount\" type=\"int32\" comment=\"max total bytes to read\" />\n"
    FIELD_NFSSTAT
    GROUP_FATTR("dir_")
    FIELD_VERF_N("new_cookieverf", "cookie verifier from reply")
    "  <field name=\"eof\" type=\"bool\" opt_nullable=\"yes\" comment=\"true if this is the end of the directory\" />\n"
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_READDIR_ENTRIES =
    "<ExtentType name=\"trace::rpc::nfs3::readdir::entries\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFNAME("filename", "")
    "  <field name=\"cookie\" type=\"int64\" comment=\"readdir cookie\" />\n"
    GROUP_FATTR("")
    FIELD_NFSFH_N("filehandle", "filehandle of the named file")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_FSSTAT =
    "<ExtentType name=\"trace::rpc::nfs3::fsstat\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "handle within file system to query")
    FIELD_NFSSTAT
    GROUP_FATTR("")
    "  <field name=\"tbytes\" type=\"int64\" opt_nullable=\"yes\" comment=\"total size of file system in bytes\" />\n"
    "  <field name=\"fbytes\" type=\"int64\" opt_nullable=\"yes\" comment=\"free space in bytes\" />\n"
    "  <field name=\"abytes\" type=\"int64\" opt_nullable=\"yes\" comment=\"free space available to user in bytes\" />\n"
    "  <field name=\"tfiles\" type=\"int64\" opt_nullable=\"yes\" comment=\"total inode slots\" />\n"
    "  <field name=\"ffiles\" type=\"int64\" opt_nullable=\"yes\" comment=\"free inode slots\" />\n"
    "  <field name=\"afiles\" type=\"int64\" opt_nullable=\"yes\" comment=\"free inode slots available to the user\" />\n"
    "  <field name=\"invarsec\" type=\"int32\" opt_nullable=\"yes\" comment=\"number of seconds for which the file system is not expected to change\" />\n"
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_FSINFO =
    "<ExtentType name=\"trace::rpc::nfs3::fsinfo\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "handle within file system to query")
    FIELD_NFSSTAT
    GROUP_FATTR("")
    "  <field name=\"rtmax\" type=\"int32\" opt_nullable=\"yes\" comment=\"max read size in bytes\" />\n"
    "  <field name=\"rtpref\" type=\"int32\" opt_nullable=\"yes\" comment=\"preferred read size in bytes\" />\n"
    "  <field name=\"rtmult\" type=\"int32\" opt_nullable=\"yes\" comment=\"preferred read size multiple in bytes\" />\n"
    "  <field name=\"wtmax\" type=\"int32\" opt_nullable=\"yes\" comment=\"max write size in bytes\" />\n"
    "  <field name=\"wtpref\" type=\"int32\" opt_nullable=\"yes\" comment=\"preferred write size in bytes\" />\n"
    "  <field name=\"wtmult\" type=\"int32\" opt_nullable=\"yes\" comment=\"preferred write size multiple in bytes\" />\n"
    "  <field name=\"dtpref\" type=\"int32\" opt_nullable=\"yes\" comment=\"preferred size of readdir requests\" />\n"
    "  <field name=\"maxfilesize\" type=\"int64\" opt_nullable=\"yes\" comment=\"max size of a file in bytes\" />\n"
    "  <field name=\"time_delta\" type=\"int64\" opt_nullable=\"yes\" comment=\"server time granularity in ns\" />\n"
    "  <field name=\"properties\" type=\"int32\" opt_nullable=\"yes\" print_format=\"%08x\" comment=\"bitmask of file system properties\" />\n"
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_PATHCONF =
    "<ExtentType name=\"trace::rpc::nfs3::pathconf\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("filehandle", "handle within file system to query")
    FIELD_NFSSTAT
    GROUP_FATTR("")
    "  <field name=\"linkmax\" type=\"int32\" opt_nullable=\"yes\" comment=\"max number of hard links to a file\" />\n"
    "  <field name=\"namemax\" type=\"int32\" opt_nullable=\"yes\" comment=\"max length of a component of a file name\" />\n"
    "  <field name=\"no_trunc\" type=\"bool\" opt_nullable=\"yes\" comment=\"if true, names longer than namemax will return error\" />\n"
    "  <field name=\"chown_restricted\" type=\"bool\" opt_nullable=\"yes\" comment=\"if true, server will reject chown if caller is not uid 0\" />\n"
    "  <field name=\"case_insensitive\" type=\"bool\" opt_nullable=\"yes\" comment=\"if true, server does not distinguish case of file names\" />\n"
    "  <field name=\"case_preserving\" type=\"bool\" opt_nullable=\"yes\" comment=\"if true, server will preserve the case of file names\" />\n"
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_NFS3_COMMIT =
    "<ExtentType name=\"trace::rpc::nfs3::commit\" namespace=\"atg.netapp.com\" version=\"1.0\">\n"
    GROUP_RECORD_ID
    FIELD_NFSFH("file", "file to be committed")
    "  <field name=\"offset\" type=\"int64\" comment=\"file offset in bytes\" />\n"
    "  <field name=\"count\" type=\"int32\" comment=\"number of bytes to read/write\" />\n"
    FIELD_NFSSTAT
    GROUP_WCC_DATA("")
    FIELD_VERF_N("writeverf", "write verifier")
    "</ExtentType>\n";

const char *EXTENT_CHRONICLE_STATS = 
    "<ExtentType name=\"chronicle::stats\" namespace=\"atg.netapp.com\" version=\"1.0\" >\n"
    FIELD_TIME("time", "ending time of the stats interval in ")
    "  <field name=\"object\" type=\"variable32\" comment=\"name of the statistic being logged\" pack_unique=\"yes\" print_style=\"text\" />\n"
    "  <field name=\"value\" type=\"int64\" comment=\"value of the statistic\" />\n"
    "</ExtentType>\n";
