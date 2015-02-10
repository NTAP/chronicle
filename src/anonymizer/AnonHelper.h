// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef ANONHELPER_H
#define ANONHELPER_H

#include <list>
#include <string>

typedef std::list<std::string> StringList;

/// Tokenize a string by breaking it at the provided separators.
StringList tokenize(const std::string &inString, char separator);

#endif // ANONHELPER_H
