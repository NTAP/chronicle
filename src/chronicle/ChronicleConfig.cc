// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleConfig.h"

std::string traceDirectory(TRACE_DIRECTORY);
off64_t dsFileSize = DS_DEFAULT_FILE_SIZE;
unsigned dsExtentSize = DS_DEFAULT_EXTENT_SIZE;
bool dsEnableIpChecksum = DS_DEFAULT_ENABLE_IP_CHECKSUM;

