// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef NULLANONYMIZER_H
#define NULLANONYMIZER_H

#include "DataSeries/GeneralField.hpp"
#include "FieldAnonymizer.h"

/**
 * Anonymizer that sets a nullable field to null.
 */
class NullAnonymizer : public FieldAnonymizer {
public:
    NullAnonymizer(DataSeriesModule *source,
                   const std::string &extentTypeName,
                   const std::string &fieldName) :
        FieldAnonymizer(source, extentTypeName, fieldName) { }

protected:
    virtual void anonymizeField(GeneralField *field) {
        field->setNull();
    }
};

#endif // NULLANONYMIZER_H
