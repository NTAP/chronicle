// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "PathAnonymizer.h"
#include "AnonHelper.h"
#include <sstream>

PathAnonymizer::PathAnonymizer(Anonymizer *anonymizer,
                               const std::string &pathSeparator) :
    _anonymizer(anonymizer), _separator(pathSeparator)
{
}

PathAnonymizer::~PathAnonymizer()
{
    if (_anonymizer)
        delete _anonymizer;
}


std::string
PathAnonymizer::anonymize(const std::string &inData)
{
    if (inData.size() == 0) {
        return inData;
    }

    StringList components = tokenize(inData, _separator[0]);
    std::ostringstream outPath;
    for (StringList::const_iterator i = components.begin();
         i != components.end(); ++i) {
        if (i->size()) {
            if (*i == "." || *i == "..") { // pass through . & ..
                outPath << *i;
            } else { // anonymize everything else
                outPath << _anonymizer->anonymize(*i);
            }
        }
        outPath << _separator;
    }

    std::string out = outPath.str();
    // remove final trailing separator
    out.pop_back();
    return out;
}
