// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef EXTENTSTRIP_H
#define EXTENTSTRIP_H

#include "DataSeries/DataSeriesModule.hpp"

/**
 * DataSeries module that serves as a base for anonymization
 * modules. This module is very similar to the RowAnalysisModule
 * except that it allows the specification of an extent type
 * name. Extents with an (exactly) matching name will be processed,
 * and those that don't match will be passed through transparently.
 */
class ExtentStrip : public DataSeriesModule {
public:
    /// Construct an anonymizer module
    /// @param[in] source The module from which this one will obtain
    /// extents
    /// @param[in] extentTypeName The name of the extents that we want
    /// to strip from the stream
    ExtentStrip(DataSeriesModule *source, const std::string &extentTypeName) :
        _source(source), _extentTypeName(extentTypeName),
        _matchedOneExtent(false) { }
    virtual ~ExtentStrip() {
        if (!_matchedOneExtent)
            std::cout << "# WARNING: No extents of type: '" << _extentTypeName
                      << "' found during anonymization!" << std::endl;
    }
    virtual Extent::Ptr getSharedExtent() {
        Extent::Ptr extent;
        while (true) {
            extent = _source->getSharedExtent();
            if (!extent || extent->getTypePtr()->getName() != _extentTypeName)
                return extent;
            _matchedOneExtent = true;
        }
    }

private:
    /// The source module
    DataSeriesModule *_source;
    /// The name of the extent we are matching against
    const std::string _extentTypeName;
    bool _matchedOneExtent;
};

#endif // EXTENTSTRIP_H
