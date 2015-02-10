// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef SCANNERFACTORY_H
#define SCANNERFACTORY_H

#include <string>
#include "DirScanner.h"

class DirGatherer;
class StatGatherer;

class ScannerFactory {
public:
    ScannerFactory();
    void statGatherer(StatGatherer *sg);
    void dirGatherer(DirGatherer *dg);

    DirScanner *newScanner(std::string &path, DirScanner::CompletionCb *cb);
private:
    StatGatherer *_sg;
    DirGatherer *_dg;
};

#endif  // SCANNERFACTORY_H
