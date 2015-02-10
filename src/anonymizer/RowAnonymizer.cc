// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "RowAnonymizer.h"
#include <iostream>

RowAnonymizer::RowAnonymizer(DataSeriesModule *source,
                             const std::string &extentTypeName) :
    _source(source), _extentTypeName(extentTypeName), _matchedOneExtent(false)
{
}


RowAnonymizer::~RowAnonymizer()
{
    if (!_matchedOneExtent)
        std::cout << "# WARNING: No extents of type: '" << _extentTypeName
                  << "' found during anonymization!" << std::endl;
}


Extent::Ptr
RowAnonymizer::getSharedExtent()
{
    _extent = _source->getSharedExtent();
    if (_extent == 0 ||
        _extent->getTypePtr()->getName() != _extentTypeName) {
        return _extent;
    }

    _matchedOneExtent = true;
    _series.setExtent(_extent);

    beginExtent();
    for(; _series.morerecords(); ++_series) {
        processRow();
    }
    endExtent();

    return _extent;
}

void
RowAnonymizer::beginExtent()
{
    // dummy
}

void
RowAnonymizer::endExtent()
{
    // dummy
}
