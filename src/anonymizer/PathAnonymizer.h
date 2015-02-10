// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PATHANONYMIZER_H
#define PATHANONYMIZER_H

#include "Anonymizer.h"

/**
 * Anonymizer that anonymizes pathnames by splitting it into
 * components and using an Anonymizer on each component.
 */
class PathAnonymizer : public Anonymizer {
public:
    PathAnonymizer(Anonymizer *anonymizer,
                   const std::string &pathSeparator = "/");
    ~PathAnonymizer();

    virtual std::string anonymize(const std::string &inData);

protected:
    Anonymizer *_anonymizer;
    std::string _separator;
};

#endif // PATHANONYMIZER_H
