// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "PcapFile.h"
#include <arpa/inet.h>
#include <cassert>
#include <iostream>

// pcapDuplicate outfile ipaddr start# count infiles...

int
main(int argc, char *argv[])
{
    unsigned packets = 0;
    unsigned modPackets = 0;

    assert(argc > 5);

    std::string outfile(argv[1]);
    uint32_t ipAddr = atoi(argv[2]);
    unsigned start = atoi(argv[3]);
    unsigned count = atoi(argv[4]);

    PcapFileReader r;
    for (int i = 5; i < argc; ++i) {
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
        // 0-5 esrc, 6-11 edst, 12-13 len, payload start @ 14
        // ip src @ 26, ip dst @ 30
        if (h.snaplen >= 34) {
            uint32_t *addr = reinterpret_cast<uint32_t *>(&packet[26]);
            if (ntohl(*addr) == ipAddr) {
                for (unsigned i = 0; i < count; ++i) {
                    *addr = htonl(i+start);
                    w.writePacket(&h);
                    ++modPackets;
                }
            }
            addr = reinterpret_cast<uint32_t *>(&packet[30]);
            if (ntohl(*addr) == ipAddr) {
                for (unsigned i = 0; i < count; ++i) {
                    *addr = htonl(i+start);
                    w.writePacket(&h);
                    ++modPackets;
                }
            }
        }
        len = sizeof(packet);
    }

    std::cout << "Read " << packets << " packets" << std::endl;
    std::cout << "Wrote " << modPackets << " packets" << std::endl;

    return 0;
}
