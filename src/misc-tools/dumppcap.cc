// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "PcapFile.h"
#include <iostream>

int
main(int argc, char *argv[])
{
    PcapFileReader r;
    for (int i = 1; i < argc; ++i) {
        r.addSource(argv[i]);
    }

    PacketHeader h;
    while (r.readPacket(&h, 0, 0)) {
        std::cout << h << std::endl;
    }

    return 0;
}
