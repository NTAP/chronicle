// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include <endian.h>
#include "ChronicleConfig.h"

// flags for different types of packets
#define	PACKET_OUT_OF_ORDER					(1)
#define PACKET_TRUNCATED					(1 << 1)
#define PACKET_SCANNED						(1 << 2)
#define PACKET_RETRANS						(1 << 3)

// macros for converting big endian representations to the host representation
#if __BYTE_ORDER == LITTLE_ENDIAN
#define UINT16_BIG2HOST(ethFrame, index)									\
	ethFrame[index] << 8 | ethFrame[index+1]
#define UINT32_BIG2HOST(ethFrame, index)									\
	(UINT16_BIG2HOST(ethFrame, index)) << 16 | UINT16_BIG2HOST(ethFrame, index+2)		
#define UINT64_BIG2HOST(ethFrame, index)									\
	(UINT32_BIG2HOST(ethFrame, index)) << 32 | UINT32_BIG2HOST(ethFrame, index+4)
#else
#define UINT16_BIG2HOST(ethFrame, index)									\
	ethFrame[index] | ethFrame[index+1] << 8
#define UINT32_BIG2HOST(ethFrame, index)									\
	UINT16_BIG2HOST(ethFrame, index) | (UINT16_BIG2HOST(ethFrame, index+2)) << 16		
#define UINT64_BIG2HOST(ethFrame, index)									\
	UINT32_BIG2HOST(ethFrame, index) | (UINT32_BIG2HOST(ethFrame, index+4)) << 32
#endif

class PcapPacketBuffer {
	public:
		PcapPacketBuffer() { } 
		~PcapPacketBuffer() { }
		unsigned char *getEthFrameAddress() { return ethFrame; }
		unsigned char ethFrame[MAX_ETH_FRAME_SIZE_STAND];
};

class PcapPacketBufferJumbo {
	public:
		PcapPacketBufferJumbo() { } 
		~PcapPacketBufferJumbo() { }
		unsigned char *getEthFrameAddress() { return ethFrame; }
		unsigned char ethFrame[MAX_ETH_FRAME_SIZE_JUMBO];
};

class NetmapPacketBuffer {
	public:
		unsigned char *getEthFrameAddress() { return ethFrame; }	
		unsigned char *ethFrame;
};

#endif //PACKET_BUFFER_H
