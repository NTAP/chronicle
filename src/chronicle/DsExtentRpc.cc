// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "ChronicleExtents.h"
#include "DsExtentRpc.h"
#include "DsWriter.h"
#include <linux/nfs.h>
#include <linux/nfs3.h>
#include <arpa/inet.h>

static const char *
rpcOpToName(uint32_t rpcProgram, uint32_t version, uint32_t procedure)
{
    const char *name = "unknown";
    if (rpcProgram == NFS_PROGRAM && version == NFS3_VERSION) {
        // NFS v3
        switch (procedure) {
        case NFS3PROC_NULL: name = "null"; break;
        case NFS3PROC_GETATTR: name = "getattr"; break;
        case NFS3PROC_SETATTR: name = "setattr"; break;
        case NFS3PROC_LOOKUP: name = "lookup"; break;
        case NFS3PROC_ACCESS: name = "access"; break;
        case NFS3PROC_READLINK: name = "readlink"; break;
        case NFS3PROC_READ: name = "read"; break;
        case NFS3PROC_WRITE: name = "write"; break;
        case NFS3PROC_CREATE: name = "create"; break;
        case NFS3PROC_MKDIR: name = "mkdir"; break;
        case NFS3PROC_SYMLINK: name = "symlink"; break;
        case NFS3PROC_MKNOD: name = "mknod"; break;
        case NFS3PROC_REMOVE: name = "remove"; break;
        case NFS3PROC_RMDIR: name = "rmdir"; break;
        case NFS3PROC_RENAME: name = "rename"; break;
        case NFS3PROC_LINK: name = "link"; break;
        case NFS3PROC_READDIR: name = "readdir"; break;
        case NFS3PROC_READDIRPLUS: name = "readdirplus"; break;
        case NFS3PROC_FSSTAT: name = "fsstat"; break;
        case NFS3PROC_FSINFO: name = "fsinfo"; break;
        case NFS3PROC_PATHCONF: name = "pathconf"; break;
        case NFS3PROC_COMMIT: name = "commit"; break;
        }
    }
    return name;
}

DsExtentRpc::DsExtentRpc() :
    _rpcRequestAt(_esRpc, "request_at", Field::flag_nullable),
    _rpcReplyAt(_esRpc, "reply_at", Field::flag_nullable),
    _rpcClient(_esRpc, "client"),
    _rpcClientPort(_esRpc, "client_port"),
    _rpcServer(_esRpc, "server"),
    _rpcServerPort(_esRpc, "server_port"),
    _rpcIsUdp(_esRpc, "is_udp"),
    _rpcRpcProgram(_esRpc, "rpc_program"),
    _rpcProgramVersion(_esRpc, "program_version"),
    _rpcProgramProc(_esRpc, "program_proc"),
    _rpcOperation(_esRpc, "operation"),
    _rpcTransactionId(_esRpc, "transaction_id"),
    _rpcCallHeaderOffset(_esRpc, "call_header_offset", Field::flag_nullable),
    _rpcCallPayloadLength(_esRpc, "call_payload_length", Field::flag_nullable),
    _rpcReplyHeaderOffset(_esRpc, "reply_header_offset", Field::flag_nullable),
    _rpcReplyPayloadLength(_esRpc, "reply_payload_length", Field::flag_nullable),
    _rpcRecordId(_esRpc, "record_id")
{
}

void
DsExtentRpc::attach(DataSeriesSink *outfile,
                    ExtentTypeLibrary *lib,
                    unsigned extentSize)
{
    ExtentType::Ptr etRpc(lib->registerTypePtr(EXTENT_CHRONICLE_RPCCOMMON));
    _om = new OutputModule(*outfile, _esRpc, etRpc, extentSize);
}

void
DsExtentRpc::write(PduDescriptor *rpcCall, PduDescriptor *rpcReply)
{
    _om->newRecord();

    // Write out the RPC call descriptor info, if any
    if(rpcCall) {

        PacketDescriptor *callPktDesc = rpcCall->firstPktDesc;
        const pcap_pkthdr *callHdr = &callPktDesc->pcapHeader;

        _rpcRequestAt.set(DsWriter::tvToNs(callHdr->ts));
        _rpcClient.set(callPktDesc->srcIP);
        _rpcClientPort.set(callPktDesc->srcPort);
        _rpcServer.set(callPktDesc->destIP);
        _rpcServerPort.set(callPktDesc->destPort);
        _rpcIsUdp.set(callPktDesc->protocol == IPPROTO_UDP);
        _rpcCallHeaderOffset.set(rpcCall->rpcHeaderOffset);
        _rpcCallPayloadLength.set(rpcCall->rpcPduLen);
    } else {
        _rpcRequestAt.setNull();
        _rpcCallHeaderOffset.setNull();
        _rpcCallPayloadLength.setNull();
    }

    //const unsigned RPC_CALL = 0;

    // Write out the RPC reply descriptor info, if any
    if(rpcReply) {

        PacketDescriptor *pktDesc = rpcReply->firstPktDesc;
        const pcap_pkthdr *hdr = &pktDesc->pcapHeader;

        _rpcReplyAt.set(DsWriter::tvToNs(hdr->ts));
        _rpcServer.set(pktDesc->srcIP);
        _rpcServerPort.set(pktDesc->srcPort);
        _rpcClient.set(pktDesc->destIP);
        _rpcClientPort.set(pktDesc->destPort);
        _rpcIsUdp.set(pktDesc->protocol == IPPROTO_UDP);
        _rpcReplyHeaderOffset.set(rpcReply->rpcHeaderOffset);
        _rpcReplyPayloadLength.set(rpcReply->rpcPduLen);
    } else {
        _rpcReplyAt.setNull();
        _rpcReplyHeaderOffset.setNull();
        _rpcReplyPayloadLength.setNull();
    }
        

    // Write out RPC call-reply common info
    PduDescriptor *pduDesc = (rpcCall)? rpcCall:rpcReply;

    _rpcRpcProgram.set(pduDesc->rpcProgram);
    _rpcProgramVersion.set(pduDesc->rpcProgramVersion);
    _rpcProgramProc.set(pduDesc->rpcProgramProcedure);
    _rpcOperation.set(rpcOpToName(pduDesc->rpcProgram,
                                  pduDesc->rpcProgramVersion,
                                  pduDesc->rpcProgramProcedure));
    _rpcTransactionId.set(pduDesc->rpcXid);

    _rpcRecordId.set(pduDesc->dsRecordId);
}

