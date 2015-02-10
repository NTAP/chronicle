// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "AnonHelper.h"

StringList
tokenize(const std::string &inString, char separator)
{
    StringList tokens;
    std::string::size_type start = 0;
    std::string::size_type end;
    do {
        end = inString.find(separator, start);
        tokens.push_back(inString.substr(start, end - start));
        start = end + 1;
    } while (std::string::npos != end);

    return tokens;
}
