// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ScannerFactory.h"
#include "DirScanner.h"

ScannerFactory::ScannerFactory() :
    _sg(NULL), _dg(NULL)
{
}
void
ScannerFactory::statGatherer(StatGatherer *sg)
{
    _sg = sg;
}

void
ScannerFactory::dirGatherer(DirGatherer *dg)
{
    _dg = dg;
}

DirScanner *
ScannerFactory::newScanner(std::string &path, DirScanner::CompletionCb *cb)
{
    return new DirScanner(path, _sg, _dg, cb);
}
