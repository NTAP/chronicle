// -*- mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLEEXTENTS_H
#define CHRONICLEEXTENTS_H

/**
 * @file
 * This file defines the DataSeries extent types that are used by Chronicle.
 */

extern const char *EXTENT_CHRONICLE_IP;

/// The RPC extent. There is one of these records for each SUNRPC call
/// & reply that is captured by Chronicle.
extern const char *EXTENT_CHRONICLE_RPCCOMMON;

extern const char *EXTENT_CHRONICLE_NFS3_GETATTR;
extern const char *EXTENT_CHRONICLE_NFS3_SETATTR;
extern const char *EXTENT_CHRONICLE_NFS3_LOOKUP;
extern const char *EXTENT_CHRONICLE_NFS3_ACCESS;
extern const char *EXTENT_CHRONICLE_NFS3_READLINK;
extern const char *EXTENT_CHRONICLE_NFS3_READWRITE;
extern const char *EXTENT_CHRONICLE_NFS3_RWCHECKSUM;
// create, mkdir, symlink, mknod
extern const char *EXTENT_CHRONICLE_NFS3_CREATE;
// remove, rmdir
extern const char *EXTENT_CHRONICLE_NFS3_REMOVE;
extern const char *EXTENT_CHRONICLE_NFS3_RENAME;
extern const char *EXTENT_CHRONICLE_NFS3_LINK;
// readdir & readdirplus
extern const char *EXTENT_CHRONICLE_NFS3_READDIR;
extern const char *EXTENT_CHRONICLE_NFS3_READDIR_ENTRIES;
extern const char *EXTENT_CHRONICLE_NFS3_FSSTAT;
extern const char *EXTENT_CHRONICLE_NFS3_FSINFO;
extern const char *EXTENT_CHRONICLE_NFS3_PATHCONF;
extern const char *EXTENT_CHRONICLE_NFS3_COMMIT;

extern const char *EXTENT_CHRONICLE_STATS;

#endif // CHRONICLEEXTENTS_H
