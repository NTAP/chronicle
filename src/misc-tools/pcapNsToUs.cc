// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "PcapFile.h"
#include <cassert>
#include <iostream>

int
main(int argc, char *argv[])
{
    unsigned packets = 0;

    assert(argc > 2);

    std::string outfile(argv[1]);

    PcapFileReader r;
    for (int i = 2; i < argc; ++i) {
        r.addSource(argv[i]);
    }

    pcap_file_header hdr;
    r.getFileHeader(&hdr);
    PcapFileWriter w(outfile, &hdr, false);

    PacketHeader h;
    char packet[10000];
    size_t len = sizeof(packet);
    while (r.readPacket(&h, packet, &len)) {
        ++packets;
        //std::cout << h << std::endl;
        w.writePacket(&h);
        len = sizeof(packet);
    }

    std::cout << "Copied " << packets << " packets" << std::endl;

    return 0;
}
