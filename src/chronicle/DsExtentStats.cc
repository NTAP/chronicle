// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentStats.h"
#include "DsWriter.h"

DsExtentStats::DsExtentStats() :
    _time(_es, "time"),
    _object(_es, "object"),
    _value(_es, "value")
{
}

void
DsExtentStats::attach(DataSeriesSink *outfile,
                      ExtentTypeLibrary *lib,
                      unsigned extentSize)
{
    ExtentType::Ptr
        et(lib->registerTypePtr(EXTENT_CHRONICLE_STATS));
    _om = new OutputModule(*outfile, _es, et, extentSize);
}

void
DsExtentStats::write(uint64_t time, const std::string &object, int64_t value)
{
    _om->newRecord();
    _time.set(time);
    _object.set(object);
    _value.set(value);
}
