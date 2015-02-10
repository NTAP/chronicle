// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef HASHANONYMIZER_H
#define HASHANONYMIZER_H

#include "Anonymizer.h"
#include <string>

/**
 * Anonymize data using HMAC.  The anonymizer is initialized using a
 * provided key file. The key file is hashed into the key for the HMAC
 * algorithm. Each time one of the anonymize functions is called, it
 * is anonymized by calculating the HMAC of the value using the
 * pre-determined key.
 *
 * For the fixed-size functions, the HMAC is then truncated to match
 * the length of the input data. This maintains the size of the data
 * at the expense of increased collisions. Note: you probably don't
 * want to use them!
 *
 * For the function that takes an arbitrany byte stream, it returns
 * the raw HMAC of size blockSize() in the output buffer.
 */
class HashAnonymizer : public Anonymizer {
public:
    HashAnonymizer(const std::string &keyfile);
    ~HashAnonymizer();

    virtual int8_t anonymize(int8_t inVal);
    virtual uint8_t anonymize(uint8_t inVal);
    virtual int16_t anonymize(int16_t inVal);
    virtual uint16_t anonymize(uint16_t inVal);
    virtual int32_t anonymize(int32_t inVal);
    virtual uint32_t anonymize(uint32_t inVal);
    virtual int64_t anonymize(int64_t inVal);
    virtual uint64_t anonymize(uint64_t inVal);
    virtual std::string anonymize(const std::string &inData);

private:
    unsigned char *_key;
    unsigned _keylen;
};


#endif // HASHANONYMIZER_H
