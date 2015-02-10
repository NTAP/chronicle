// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef ANONYMIZER_H
#define ANONYMIZER_H

#include <cassert>
#include <cstddef>
#include <inttypes.h>
#include <string>

/**
 * Interface class for anonymization routines. This is not implemented
 * as an abstract class so we don't force everyone to implement all
 * the routines, nor require everyone to support them all. The
 * trade-off is that there could be a runtime assert.
 */
class Anonymizer {
public:
    virtual ~Anonymizer() { }

    virtual int8_t anonymize(int8_t inVal) { notSupported(); return 0; }
    virtual uint8_t anonymize(uint8_t inVal) { notSupported(); return 0; }
    virtual int16_t anonymize(int16_t inVal) { notSupported(); return 0; }
    virtual uint16_t anonymize(uint16_t inVal) { notSupported(); return 0; }
    virtual int32_t anonymize(int32_t inVal) { notSupported(); return 0; }
    virtual uint32_t anonymize(uint32_t inVal) { notSupported(); return 0; }
    virtual int64_t anonymize(int64_t inVal) { notSupported(); return 0; }
    virtual uint64_t anonymize(uint64_t inVal) { notSupported(); return 0; }
    virtual std::string anonymize(const std::string &inData) {
        notSupported();
        return std::string();
    }

protected:
    void notSupported() {
        assert("Anonymization of this value type is not supported" && 0);
    }
};

#endif // ANONYMIZER_H
