// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef IPANONYMIZER_H
#define IPANONYMIZER_H

#include "Anonymizer.h"

/**
 * Anonymizer that replaces the top bits of an IP address with a 10.*
 * address. Addresses should be in host byte order. netmaskBits is the
 * number of upper bits to anonymize (e.g., bits=24 would map all
 * addresses into a single class C subnet).
 */
class IpAnonymizer : public Anonymizer {
public:
    IpAnonymizer(unsigned netmaskBits) :
        _bits(netmaskBits) {
        assert("netmask bits should be between 1 and 31 inclusive" &&
               32 > _bits && 0 < _bits);
    }

protected:
    virtual int32_t anonymize(int32_t inVal) {
        uint32_t address = inVal;
        uint32_t bottomMask = (1u << (32 - _bits)) - 1;
        uint32_t topMask = ~bottomMask;
        return (address & bottomMask) | (MASK_ADDRESS & topMask);

    }
    unsigned _bits;
    /// 10.0.0.1
    static const uint32_t MASK_ADDRESS =
        (10 << 24) | (0 << 16) | (0 << 8) | 1;
                                            
};

#endif // IPANONYMIZER_H
