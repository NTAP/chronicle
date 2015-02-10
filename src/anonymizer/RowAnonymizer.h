// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef ROWANONYMIZER_H
#define ROWANONYMIZER_H

#include "DataSeries/DataSeriesModule.hpp"

/**
 * DataSeries module that serves as a base for anonymization
 * modules. This module is very similar to the RowAnalysisModule
 * except that it allows the specification of an extent type
 * name. Extents with an (exactly) matching name will be processed,
 * and those that don't match will be passed through transparently.
 */
class RowAnonymizer : public DataSeriesModule {
public:
    /// Construct an anonymizer module
    /// @param[in] source The module from which this one will obtain
    /// extents
    /// @param[in] extentTypeName The name of the extents that we want
    /// to anonymize
    RowAnonymizer(DataSeriesModule *source, const std::string &extentTypeName);
    virtual ~RowAnonymizer();
    virtual Extent::Ptr getSharedExtent();

protected:
    /// Called just prior to processing any rows in the extent. This
    /// should be used if there is pre-extent-based state that needs
    /// to be initialized
    virtual void beginExtent();
    /// Called once for each row in the extent
    virtual void processRow() = 0;
    /// Called after all rows in the extent have been processed. This
    /// is the place to clean up from beginExtent()
    virtual void endExtent();

    /// The extent series we are processing. This is only guaranteed
    /// to be valid during beginExtent(), processRow(), and
    /// endExtent().
    ExtentSeries _series;
    /// The current extent.
    Extent::Ptr _extent;

private:
    /// The source module
    DataSeriesModule *_source;
    /// The name of the extent we are matching against
    const std::string _extentTypeName;
    bool _matchedOneExtent;
};

#endif // ROWANONYMIZER_H
