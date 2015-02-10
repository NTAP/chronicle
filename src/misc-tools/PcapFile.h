// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAPFILE_H
#define PCAPFILE_H

#include <ctime>
#include <iostream>
#include <fstream>
#include <list>
#include <pcap.h>
#include <string>

struct PacketHeader {
    void *packetData;
    timespec ts;
    uint32_t snaplen;
    uint32_t wirelen;

    std::string toString() const;
};

std::ostream &operator<<(std::ostream &o, const PacketHeader &ph);

class PcapFileReader {
public:
    PcapFileReader();
    ~PcapFileReader();

    void addSource(const std::string &filename);
    void getFileHeader(pcap_file_header *hdr);
    bool readPacket(PacketHeader *hdr, void *pktData, size_t *dataLen);

private:
    bool openNextFile();

    typedef std::list<std::string> StringList;
    StringList _sources;
    std::ifstream _curFile;
    bool _nsTs;
    pcap_file_header _header;
};


class PcapFileWriter {
public:
    PcapFileWriter(const std::string &filename,
                   const pcap_file_header *hdr,
                   bool nsTimestamps = false);
    ~PcapFileWriter();
    bool writePacket(PacketHeader *hdr);

private:
    std::ofstream _file;
    bool _nsTs;
};

#endif
