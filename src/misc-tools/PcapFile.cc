// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "PcapFile.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iomanip>
#include <pcap.h>
#include <sstream>

std::string
PacketHeader::toString() const
{
    std::ostringstream ss;

    ss << "@: " << ts.tv_sec << "."
       << std::setw(9) << std::setfill('0') << ts.tv_nsec
       << "  snaplen: " << snaplen
       << "  wirelen: " << wirelen
       << "  dataaddr: 0x" << std::hex << std::setw(16) << std::setfill('0')
       << packetData;

    return ss.str();
}

std::ostream &
operator<<(std::ostream &o, const PacketHeader &ph)
{
    o << ph.toString();
    return o;
}


PcapFileReader::PcapFileReader()
{
}

PcapFileReader::~PcapFileReader()
{
}

void
PcapFileReader::addSource(const std::string &filename)
{
    _sources.push_back(filename);
}

struct PcapPacketHeader {
    uint32_t sec;
    uint32_t usec; // or ns
    uint32_t caplen;
    uint32_t wirelen;
};

bool
PcapFileReader::readPacket(PacketHeader *hdr, void *pktData, size_t *dataLen)
{
    size_t toRead;
    size_t toDiscard;

    size_t len = 0;
    if (!dataLen)
        dataLen = &len;

    PcapPacketHeader phdr;
    _curFile.read(reinterpret_cast<std::istream::char_type *>(&phdr),
                  sizeof(phdr));
    if (!_curFile) {
        goto trynextfile;
    }

    hdr->ts.tv_sec = phdr.sec;
    hdr->ts.tv_nsec = phdr.usec;
    if (!_nsTs) {
        hdr->ts.tv_nsec *= 1000;
    }
    hdr->snaplen = phdr.caplen;
    hdr->wirelen = phdr.wirelen;
    hdr->packetData = pktData;
    toRead = std::min(*dataLen, (size_t)hdr->snaplen);
    toDiscard = hdr->snaplen - toRead;

    _curFile.read(reinterpret_cast<std::istream::char_type *>(pktData),
                  toRead);
    if (!_curFile) {
        goto trynextfile;
    }
    *dataLen = toRead;
    _curFile.ignore(toDiscard);
    
    return true;

trynextfile:
    if (!openNextFile()) {
        return false;
    } else {
        return readPacket(hdr, pktData, dataLen);
    }
}

bool
PcapFileReader::openNextFile()
{
    if (_sources.empty())
        return false;

    std::string name = _sources.front();
    _sources.pop_front();
    _curFile.open(name);
    if (!_curFile) {
        std::cout << "error opening: " << name << std::endl;
        return openNextFile();
    }

    _curFile.read(reinterpret_cast<std::istream::char_type *>(&_header),
                  sizeof(_header));
    if (!_curFile) {
        std::cout << "error reading header of: " << name << std::endl;
        return openNextFile();
    }

    switch (_header.magic) {
    case 0xa1b2c3d4:
        _nsTs = false;
        std::cout << "file is usec" << std::endl;
        break;
    case 0xa1b23c4d:
        _nsTs = true;
        std::cout << "file is nsec" << std::endl;
        break;
    default:
        std::cout << "unknown magic number in: " << name << std::endl;
        return openNextFile();
    };

    return true;
}

void
PcapFileReader::getFileHeader(pcap_file_header *hdr)
{
    if (!_curFile.is_open()) {
        openNextFile();
    }
    memcpy(hdr, &_header, sizeof(_header));
}




PcapFileWriter::PcapFileWriter(const std::string &filename,
                               const pcap_file_header *hdr,
                               bool nsTimestamps) :
    _file(filename), _nsTs(nsTimestamps)
{
    pcap_file_header header;
    memcpy (&header, hdr, sizeof(header));
    if (_nsTs) {
        header.magic = 0xa1b23c4d;
    } else {
        header.magic = 0xa1b2c3d4;
    }

    _file.write(reinterpret_cast<const std::ostream::char_type *>(&header),
                sizeof(header));
    if (!_file) {
        std::cout << "error writing pcap header to: " << filename << std::endl;
    }
}


PcapFileWriter::~PcapFileWriter()
{
}

bool
PcapFileWriter::writePacket(PacketHeader *hdr)
{
    PcapPacketHeader pcaphdr;
    pcaphdr.sec = hdr->ts.tv_sec;
    if (_nsTs) {
        pcaphdr.usec = hdr->ts.tv_nsec;
    } else {
        pcaphdr.usec = hdr->ts.tv_nsec / 1000;
    }
    pcaphdr.caplen = hdr->snaplen;
    pcaphdr.wirelen = hdr->wirelen;
    _file.write(reinterpret_cast<const std::ostream::char_type *>(&pcaphdr),
                sizeof(pcaphdr));
    _file.write(reinterpret_cast<const std::ostream::char_type *>(hdr->packetData),
                pcaphdr.caplen);
    if (!_file) {
        std::cout << "error writing pcap packet" << std::endl;
        return false;
    }

    return true;
}
