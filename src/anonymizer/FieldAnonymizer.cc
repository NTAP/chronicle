// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "FieldAnonymizer.h"

FieldAnonymizer::FieldAnonymizer(DataSeriesModule *source,
                                 const std::string &extentTypeName,
                                 const std::string &fieldName) :
    RowAnonymizer(source, extentTypeName), _fieldName(fieldName), _field(0)
{
}

FieldAnonymizer::~FieldAnonymizer()
{
    if (_field)
        delete _field;
}

void
FieldAnonymizer::beginExtent()
{
    if (!_field)
        _field = GeneralField::create(_series, _fieldName);
}

void
FieldAnonymizer::processRow()
{
    anonymizeField(_field);
}


GenericFieldAnonymizer::GenericFieldAnonymizer(DataSeriesModule *source,
                                               const std::string &extentTypeName,
                                               const std::string &fieldName,
                                               Anonymizer *anonymizer) :
    FieldAnonymizer(source, extentTypeName, fieldName), _anonymizer(anonymizer)
{
}

GenericFieldAnonymizer::~GenericFieldAnonymizer()
{
    if (_anonymizer)
        delete _anonymizer;
}

void
GenericFieldAnonymizer::anonymizeField(GeneralField *field)
{
    if (field->isNull()) return;

    GeneralValue gv(*field);
    //std::cout << "val in: " << gv;

    switch (field->getType()) {
    case ExtentType::ft_byte:
        gv.setByte(_anonymizer->anonymize(gv.valByte()));
        break;
    case ExtentType::ft_int32:
        gv.setInt32(_anonymizer->anonymize(gv.valInt32()));
        break;
    case ExtentType::ft_int64:
        gv.setInt64(_anonymizer->anonymize(gv.valInt64()));
        break;
    case ExtentType::ft_variable32: {
        gv.setVariable32(_anonymizer->anonymize(gv.valString()));
        break;
    }
    case ExtentType::ft_double:
    case ExtentType::ft_fixedwidth:
    default:
        assert("Don't know how to anonymize field type" && 0);
    };

    field->set(gv);
    //std::cout << "   field out: " << *field << std::endl;
}
