// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef FIELDANONYMIZER_H
#define FIELDANONYMIZER_H

#include "Anonymizer.h"
#include "DataSeries/GeneralField.hpp"
#include "RowAnonymizer.h"

/**
 * Class for an anonymizer than changes a single field.
 */
class FieldAnonymizer : public RowAnonymizer {
public:
    FieldAnonymizer(DataSeriesModule *source,
                    const std::string &extentTypeName,
                    const std::string &fieldName);
    virtual ~FieldAnonymizer();

protected:
    /// This should be overridden to anonymize the provided
    /// GeneralField. Called once per row.
    /// @param[inout] field The field to anonymize
    virtual void anonymizeField(GeneralField *field) = 0;

private:
    virtual void beginExtent();
    virtual void processRow();

    std::string _fieldName;
    GeneralField *_field;
};


class GenericFieldAnonymizer : public FieldAnonymizer {
public:
    GenericFieldAnonymizer(DataSeriesModule *source,
                           const std::string &extentTypeName,
                           const std::string &fieldName,
                           Anonymizer *anonymizer);
    ~GenericFieldAnonymizer();

private:
    virtual void anonymizeField(GeneralField *field);

    Anonymizer *_anonymizer;
};

#endif // FIELDANONYMIZER_H
