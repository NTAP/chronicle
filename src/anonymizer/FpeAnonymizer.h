// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2014 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FPEANONYMIZER_H
#define FPEANONYMIZER_H

#include "Anonymizer.h"
#include "RandCache.h"
#include <string>

/**
 * Anonymizer that permutes the values in a field based on a key and
 * tweak. This anonymizer uses Format Preserving Encryption (FPE)
 * based on the FF1 mode from NIST 800-38G draft.
 */
class FpeAnonymizer : public Anonymizer {
public:
    FpeAnonymizer(const std::string &keyfile,
                  const std::string &tweak);
    ~FpeAnonymizer();

protected:
    virtual int32_t anonymize(int32_t inVal);
    virtual int64_t anonymize(int64_t inVal);

private:
    // Encrypt the data in val using the FF1 mode of 800-38G.
    void ff1Encrypt(uint64_t *val, unsigned bytes);

    RandCache<int32_t, int32_t> _cache32;
    RandCache<int64_t, int64_t> _cache64;
    unsigned char *_key;
    unsigned char *_tweak;
    unsigned _tweaklen;
};

#endif // FPEANONYMIZER_H
