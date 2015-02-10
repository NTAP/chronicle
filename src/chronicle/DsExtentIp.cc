// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "Interface.h"
#include "DsExtentIp.h"
#include "DsWriter.h"
#include <arpa/inet.h>

DsExtentIp::DsExtentIp() :
    _ipInterface(_esIP, "interface"),
    _ipRingNum(_esIP, "ring_num"),
    _ipPacketAt(_esIP, "packet_at"),
    _ipSource(_esIP, "source"),
    _ipSourcePort(_esIP, "source_port"),
    _ipSourceMac(_esIP, "source_mac"),
    _ipDest(_esIP, "dest"),
    _ipDestPort(_esIP, "dest_port"),
    _ipDestMac(_esIP, "dest_mac"),
    _ipWireLength(_esIP, "wire_length"),
    _ipProtocol(_esIP, "protocol"),
    _ipIsFragment(_esIP, "is_fragment"),
    _ipTcpSeqnum(_esIP, "tcp_seqnum", Field::flag_nullable),
    _ipTcpPayloadLength(_esIP, "tcp_payload_length", Field::flag_nullable),
    _ipIsGoodPdu(_esIP, "is_good_pdu"),
    _ipRecordId(_esIP, "record_id")
{
}

void
DsExtentIp::attach(DataSeriesSink *outfile,
                   ExtentTypeLibrary *lib,
                   unsigned extentSize)
{
    ExtentType::Ptr etIP(lib->registerTypePtr(EXTENT_CHRONICLE_IP));
    _om = new OutputModule(*outfile, _esIP, etIP, extentSize);
}

void
DsExtentIp::write(PacketDescriptor *p, uint64_t recordId, bool isGoodPdu)
{
    _om->newRecord();

    const pcap_pkthdr *hdr = &p->pcapHeader;
    _ipInterface.set(p->interface->getName());
    _ipRingNum.set(p->ringId);
    _ipPacketAt.set(DsWriter::tvToNs(hdr->ts));
    _ipSource.set(p->srcIP);
    _ipSourcePort.set(p->srcPort);
    _ipSourceMac.set(&((p->getEthFrameAddress())[6]), 6);
    _ipDest.set(p->destIP);
    _ipDestPort.set(p->destPort);
    _ipDestMac.set(p->getEthFrameAddress(), 6);
    _ipWireLength.set(hdr->len);
    _ipProtocol.set(p->protocol);
    switch (p->protocol) {
    case IPPROTO_TCP:
        _ipTcpSeqnum.set(p->tcpSeqNum);
        _ipTcpPayloadLength.set(p->payloadLength);
        break;
    case IPPROTO_UDP:
    default:
        _ipTcpSeqnum.setNull();
        _ipTcpPayloadLength.setNull();
        break;
    }
    // XXX get this info!
    _ipIsFragment.set(false);
    _ipIsGoodPdu.set(isGoodPdu);
    _ipRecordId.set(recordId);
}
