// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FILEEXTANONYMIZER_H
#define FILEEXTANONYMIZER_H

#include "DataSeries/GeneralField.hpp"
#include "Anonymizer.h"
#include "HashAnonymizer.h"

/**
 * Anonymizer that changes a filename into
 * "<length>|<basehash>|extension". For example, the file: "file.txt"
 * could hash into
 * "0008|9bf908850d59279e7ff1513cee29e4d30c79a8d5|txt". The length of
 * the original name is 0x0008, the hash of "file" is 9b...d5, and the
 * extension is "txt".
 */
class FileExtAnonymizer : public Anonymizer {
public:
    FileExtAnonymizer(Anonymizer *textHasher);
    ~FileExtAnonymizer();

    virtual std::string anonymize(const std::string &inData);
protected:

    /// Splits a filename into its basename and its extension. Either
    /// one may be empty.
    std::pair<std::string, std::string>
    fileAndExtension(const std::string &filename);

    Anonymizer *_hasher;
};

#endif // FILEEXTANONYMIZER_H
