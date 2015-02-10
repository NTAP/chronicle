// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "FileExtAnonymizer.h"
#include <iomanip>
#include <sstream>

FileExtAnonymizer::FileExtAnonymizer(Anonymizer *textHasher) :
    _hasher(textHasher)
{
}

FileExtAnonymizer::~FileExtAnonymizer()
{
    if (_hasher)
        delete _hasher;
}

std::string
FileExtAnonymizer::anonymize(const std::string &inData)
{
    std::ostringstream outName;

    // write size of the original filename
    outName << std::hex << std::setw(4)
            << std::setfill('0') << inData.size();

    std::pair<std::string, std::string> parts = fileAndExtension(inData);

    // hash the basename
    outName << "|";
    std::string hash(_hasher->anonymize(parts.first));
    // print it 1 byte at a time, converting to hex
    for (unsigned i = 0; i < hash.size(); ++i) {
        unsigned char ch = hash[i];
        outName << std::hex << std::setw(2)
                << static_cast<unsigned>(ch);
    }

    // Add the original extension
    outName << "|" << parts.second;

    return outName.str();
}

std::pair<std::string, std::string>
FileExtAnonymizer::fileAndExtension(const std::string &filename)
{
    std::string::size_type dotPos = filename.find_last_of(".");

    std::string basename, extension;
    if (dotPos == std::string::npos) { // no "."
        basename = filename;
        extension = "";
    } else if (dotPos == filename.size()-1) { // ends w/ "."
        basename = filename.substr(0, filename.size()-1);
        extension = "";
    } else {
        basename = filename.substr(0, dotPos);
        extension = filename.substr(dotPos+1, std::string::npos);
    }

    return make_pair(basename, extension);
}

