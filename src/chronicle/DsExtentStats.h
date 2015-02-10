// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef DSEXTENTSTATS_H
#define DSEXTENTSTATS_H

#include "ChronicleProcessRequest.h"
#include "DsExtentWriter.h"

class DsExtentStats : public DsExtentWriter {
public:
    DsExtentStats();
    virtual void attach(DataSeriesSink *outfile,
                        ExtentTypeLibrary *lib,
                        unsigned extentSize);
    void write(uint64_t time, const std::string &object, int64_t value);
    
private:
    ExtentSeries _es;
    Int64Field _time;
    Variable32Field _object;
    Int64Field _value;
};

#endif // DSEXTENTSTATS_H
