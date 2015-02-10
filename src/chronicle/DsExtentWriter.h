// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTWRITER_H
#define DSEXTENTWRITER_H

#include "DataSeries/DataSeriesModule.hpp"

/**
 * An abstract class to encapsulate code for writing a type of DS extent.
 */
class DsExtentWriter {
public:
    DsExtentWriter() : _om(0) { }
    virtual ~DsExtentWriter() { detach(); }

    /// Attach to the specified sink, and write the extent description
    /// to the supplied library.
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize) = 0;

    /// Flush any pending records and detach from the file.
    virtual void detach() {
        if (_om) { delete _om; _om = 0; }
    }

protected:
    OutputModule *_om;
};

#endif // DSEXTENTWRITER_H

