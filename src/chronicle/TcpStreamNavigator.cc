// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstring>
#include <arpa/inet.h>
#include <endian.h>
#include "TcpStreamNavigator.h"
#include "ChronicleProcessRequest.h"

TcpStreamNavigator::TcpStreamNavigator()
{
	pduDesc = NULL;
	pktDesc = NULL;
	index = 0;
}

TcpStreamNavigator::~TcpStreamNavigator()
{

}

bool
TcpStreamNavigator::init(PacketDescriptor *pkt, uint16_t idx)
{
	startingPktDesc = pktDesc = pkt;
	ethFrame = pktDesc->getEthFrameAddress();
	packetLen = pktDesc->pcapHeader.caplen;
	index = idx;
	parsedBytes = 0;
	parsedPackets = 0;
	return index <= packetLen ? true : false;
}

bool
TcpStreamNavigator::init(PduDescriptor *pdu, uint16_t idx, PacketDescriptor *pkt)
{
	pduDesc = pdu;
	if (pkt == NULL)
		pktDesc = pduDesc->firstPktDescRpcProg;
	else
		pktDesc = pkt;
	ethFrame = pktDesc->getEthFrameAddress();
	packetLen = pktDesc->pcapHeader.caplen;
	index = idx;
	parsedBytes = 0;
	parsedPackets = 0;
	return index <= packetLen ? true : false;
}

bool
TcpStreamNavigator::getUint32Packet(uint32_t *uint32, bool scanMode)
{
	int bytesLeftInPkt;
	bytesLeftInPkt = packetLen - index;
	if (bytesLeftInPkt < 4) {
		unsigned char tmp[4];
		int bytesLeft = 4;
		if (bytesLeftInPkt > 0) {
			getBytesPacket(tmp, bytesLeftInPkt);
			bytesLeft = 4 - bytesLeftInPkt;
		}
		if (!advanceToNextPacket(scanMode))
			return false;
		if (getBytesPacket(&tmp[bytesLeftInPkt], bytesLeft, scanMode)) {
			uint32_t tmpUint32;
			tmpUint32 = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;
			*uint32 = ntohl(tmpUint32);
			return true;			
		} else				
			return false;
	}
	*uint32 = UINT32_BIG2HOST(ethFrame, index);
	index += 4;
	parsedBytes += 4;
	return true;
}

bool
TcpStreamNavigator::getUint32Pdu(uint32_t *uint32)
{
	int bytesLeftInPkt;
	if (pktDesc != pduDesc->lastPktDesc)
		bytesLeftInPkt = packetLen - index;
	else
		bytesLeftInPkt = pduDesc->rpcProgramEndOffset - index + 1;
	if (bytesLeftInPkt < 4) {
		unsigned char tmp[4];
		int bytesLeft = 4;
		if (pktDesc == pduDesc->lastPktDesc)
			return false;
		if (bytesLeftInPkt > 0) {
			getBytesPdu(tmp, bytesLeftInPkt);
			bytesLeft = 4 - bytesLeftInPkt;
		}
		advanceToNextPduPacket();
		if (getBytesPdu(&tmp[bytesLeftInPkt], bytesLeft)) {
			uint32_t tmpUint32;
			tmpUint32 = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;
			*uint32 = ntohl(tmpUint32);
			return true;			
		} else				
			return false;
	}
	*uint32 = UINT32_BIG2HOST(ethFrame, index);
	index += 4;
	return true;
}

bool
TcpStreamNavigator::getUint64Packet(uint64_t *uint64, bool scanMode)
{
	uint32_t uint32_1, uint32_2;
	if (getUint32Packet(&uint32_1, scanMode) && 
			getUint32Packet(&uint32_2, scanMode)) {
		#if __BYTE_ORDER == LITTLE_ENDIAN
		*uint64 = uint32_1;
		*uint64 = *uint64 << 32 | uint32_2;
		#else
		*uint64 = uint32_2;
		*uint64 = *uint64 << 32 | uint32_1;
		#endif
		parsedBytes += 8;
		return true;
	} else
		return false;
}

bool
TcpStreamNavigator::getUint64Pdu(uint64_t *uint64)
{
	uint32_t uint32_1, uint32_2;
	if (getUint32Pdu(&uint32_1) && getUint32Pdu(&uint32_2)) {
		#if __BYTE_ORDER == LITTLE_ENDIAN
		*uint64 = uint32_1;
		*uint64 = *uint64 << 32 | uint32_2;
		#else
		*uint64 = uint32_2;
		*uint64 = *uint64 << 32 | uint32_1;
		#endif
		return true;
	} else
		return false;
}

bool
TcpStreamNavigator::getStringPdu(unsigned char *str, uint32_t maxStringLen)
{
	uint32_t strLen, residual;
	if (!getUint32Pdu(&strLen))
		return false;
	if (strLen > maxStringLen)
		return false;
	residual = strLen & 0x3;
	if (!getBytesPdu(str, strLen))
		return false;
	str[strLen] = '\0';
	if (residual > 0 && !skipBytesPdu(4 - residual))
		return false;	
	return true;
}

bool
TcpStreamNavigator::getBytesPacket(unsigned char *bytes, uint32_t numBytes, 
	bool scanMode)
{
	int bytesLeft = numBytes;
	while (bytesLeft > 0) {
		int bytesLeftInPkt;
		bytesLeftInPkt = packetLen - index;
		if (bytesLeftInPkt >= bytesLeft) {
			memcpy(bytes + numBytes - bytesLeft, &ethFrame[index], bytesLeft);
			index += bytesLeft;
			parsedBytes += numBytes;
			return true;
		} else { // the byte array spans multiple packets
			if (pktDesc->next == NULL)
				return false;
			if (bytesLeftInPkt > 0) {
				memcpy(bytes + numBytes - bytesLeft, &ethFrame[index], 
					bytesLeftInPkt);
				bytesLeft -= bytesLeftInPkt;
			}
			if (!advanceToNextPacket(scanMode))
				return false;
		}
	}
	return false;
}

bool
TcpStreamNavigator::getBytesPdu(unsigned char *bytes, uint32_t numBytes)
{
	int bytesLeft = numBytes;
	while (bytesLeft > 0) {
		int bytesLeftInPkt;
		if (pktDesc != pduDesc->lastPktDesc)
			bytesLeftInPkt = packetLen - index;
		else
			bytesLeftInPkt = pduDesc->rpcProgramEndOffset - index + 1;
		if (bytesLeftInPkt >= bytesLeft) {
			memcpy(bytes + numBytes - bytesLeft, &ethFrame[index], bytesLeft);
			index += bytesLeft;
			return true;
		} else { // the byte array spans multiple packets
			if (pktDesc == pduDesc->lastPktDesc)
				return false;
			if (bytesLeftInPkt > 0) {
				memcpy(bytes + numBytes - bytesLeft, &ethFrame[index], 
					bytesLeftInPkt);
				bytesLeft -= bytesLeftInPkt;
			}
			advanceToNextPduPacket();
		}
	}
	return false;
}

bool
TcpStreamNavigator::skipBytesPacket(uint32_t numBytes, bool scanMode)
{
	int bytesLeft = numBytes;
	if (bytesLeft == 0)
		return true;
	while (bytesLeft > 0) {
		int bytesLeftInPkt;
		bytesLeftInPkt = packetLen - index;
		if (bytesLeftInPkt >= bytesLeft) {
			index += bytesLeft;
			parsedBytes += numBytes;
			return true;
		} else {
			if (pktDesc->next == NULL)
				return false;
			bytesLeft -= bytesLeftInPkt;
			if (!advanceToNextPacket(scanMode))
				return false;
		}
	}
	return false;
}

bool
TcpStreamNavigator::skipBytesPdu(uint32_t numBytes)
{
	int bytesLeft = numBytes;
	while (bytesLeft > 0) {
		int bytesLeftInPkt;
		if (pktDesc != pduDesc->lastPktDesc)
			bytesLeftInPkt = packetLen - index;
		else
			bytesLeftInPkt = pduDesc->rpcProgramEndOffset - index + 1;
		if (bytesLeftInPkt >= bytesLeft) {
			index += bytesLeft;
			return true;
		} else { 
			if (pktDesc == pduDesc->lastPktDesc)
				return false;
			bytesLeft -= bytesLeftInPkt;
			advanceToNextPduPacket();
		}
	}
	return false;
}

bool
TcpStreamNavigator::goBackBytesPacket(uint32_t numBytes, bool scanMode)
{
	int bytesLeft = numBytes;
	if (bytesLeft == 0)
		return true;
	while (bytesLeft > 0) {
		int bytesLeftInPkt;
		bytesLeftInPkt = index - pktDesc->payloadOffset;
		if (bytesLeftInPkt >= bytesLeft) {
			index -= bytesLeft;
			parsedBytes -= numBytes;
			return true;
		} else {
			bytesLeft -= bytesLeftInPkt;
			if (scanMode)
				pktDesc->flag &= ~PACKET_SCANNED;
			if (!goBackToPrevPacket())
				return false;
		}
	}
	return false;
}

bool
TcpStreamNavigator::advanceToNextPacket(bool scanMode)
{
	if (scanMode) {
		// has ramifications for RpcParser::scanForRpcHeader
		if (pktDesc->next != NULL && !(pktDesc->next->flag & 
				PACKET_OUT_OF_ORDER)) {
			pktDesc->flag |= PACKET_SCANNED;
		}
	}
	pktDesc = pktDesc->next;
	if (pktDesc == NULL || (pktDesc->flag & PACKET_OUT_OF_ORDER))
		return false;
	parsedPackets++;
	ethFrame = pktDesc->getEthFrameAddress();
	packetLen = pktDesc->pcapHeader.caplen;
	index = pktDesc->payloadOffset;
	if (!scanMode) { 
		// has ramifications for RpcParser::findPdu
		if (pktDesc->prev != NULL && 
				pktDesc->prev->visitCount > pktDesc->visitCount)
		pktDesc->visitCount = pktDesc->prev->visitCount;
		else
			pktDesc->visitCount++;
	} 
	return true;
}

void
TcpStreamNavigator::advanceToNextPduPacket()
{
	pktDesc = pktDesc->next;
	ethFrame = pktDesc->getEthFrameAddress();
	packetLen = pktDesc->pcapHeader.caplen;
	index = pktDesc->payloadOffset;
}

bool
TcpStreamNavigator::goBackToPrevPacket()
{
	if (pktDesc->flag & PACKET_OUT_OF_ORDER)
		return false;
	if (pktDesc == startingPktDesc)
		return false;
	pktDesc = pktDesc->prev;
	if (pktDesc == NULL)
		return false;
	parsedPackets--;
	ethFrame = pktDesc->getEthFrameAddress();
	packetLen = pktDesc->pcapHeader.caplen;
	index = packetLen;
	return true;
}


