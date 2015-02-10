// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef SNAPSHOTEXTENTS_H
#define SNAPSHOTEXTENTS_H

const char *EXTENT_FSSNAPSHOT_STAT =
    "<ExtentType name=\"fssnapshot_stat\" namespace=\"atg.netapp.com\"\n"
    "  version=\"1.0\">\n"
    "  <field name=\"p_ino\" type=\"int64\"\n"
    "    comment=\"inode number of the parent directory\" />\n"
    "  <field name=\"filename\" type=\"variable32\"\n"
    "    comment=\"name of the file\" />\n"
    "  <field name=\"ino\" type=\"int64\"\n"
    "    comment=\"inode number of the file\" />\n"
    "  <field name=\"dev\" type=\"int64\"\n"
    "    comment=\"ID of the device containing the file\" />\n"
    "  <field name=\"mode\" type=\"int32\" print_format=\"%04o\"\n"
    "    comment=\"the file's mode bits (permissions)\" />\n"
    "  <field name=\"ftype\" type=\"variable32\" print_style=\"text\"\n"
    "    pack_unique=\"yes\" comment=\"the file's type (socket, symlink,\n"
    "    regular, blockdev, directory, chardev, fifo)\" />\n"
    "  <field name=\"nlink\" type=\"int64\"\n"
    "    comment=\"number of hard links to the file\" />\n"
    "  <field name=\"uid\" type=\"int32\"\n"
    "    comment=\"user ID of the owner\" />\n"
    "  <field name=\"gid\" type=\"int32\"\n"
    "    comment=\"group ID of the owner\" />\n"
    "  <field name=\"rdev\" type=\"int64\"\n"
    "    comment=\"device ID (if special file)\" />\n"
    "  <field name=\"size\" type=\"int64\"\n"
    "    comment=\"total size (bytes)\" />\n"
    "  <field name=\"blksize\" type=\"int32\"\n"
    "    comment=\"preferred block size for I/O (bytes)\" />\n"
    "  <field name=\"blocks\" type=\"int64\"\n"
    "    comment=\"count of 512B blocks allocated\" />\n"
    "  <field name=\"ctime\" type=\"int64\"\n"
    "    comment=\"time of last status change (seconds since epoch)\" />\n"
    "  <field name=\"mtime\" type=\"int64\" pack_relative=\"ctime\"\n"
    "    comment=\"time of last modification (seconds since epoch)\" />\n"
    "  <field name=\"atime\" type=\"int64\" pack_relative=\"ctime\"\n"
    "    comment=\"time of last access (seconds since epoch)\" />\n"
    "</ExtentType>\n";

const char *EXTENT_FSSNAPSHOT_SYMLINK =
    "<ExtentType name=\"fssnapshot_symlink\" namespace=\"atg.netapp.com\"\n"
    "  version=\"1.0\">\n"
    "  <field name=\"ino\" type=\"int64\"\n"
    "    comment=\"inode number of the symlink\" />\n"
    "  <field name=\"target\" type=\"variable32\"\n"
    "    comment=\"the contents (target) of the symlink\" />\n"
    "</ExtentType>\n";
#endif // SNAPSHOTEXTENTS_H
