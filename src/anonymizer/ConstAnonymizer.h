// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CONSTANONYMIZER_H
#define CONSTANONYMIZER_H

#include "Anonymizer.h"

/**
 * Anonymizer that changes field to a constant value.
 */
class ConstAnonymizer : public Anonymizer {
public:
    ConstAnonymizer(int64_t value = 0) : _value(value) { }

protected:
    virtual int8_t anonymize(int8_t inVal) {
        return static_cast<int8_t>(_value);
    }
    virtual int16_t anonymize(int16_t inVal) {
        return static_cast<int16_t>(_value);
    }
    virtual int32_t anonymize(int32_t inVal) {
        return static_cast<int32_t>(_value);
    }
    virtual int64_t anonymize(int64_t inVal) {
        return static_cast<int32_t>(_value);
    }

    int64_t _value;
};

#endif // CONSTANONYMIZER_H
