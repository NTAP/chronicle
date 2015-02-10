// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PACKET_READER_FACTORY_H
#define PACKET_READER_FACTORY_H

#include "PacketReader.h"

class Chronicle;
class Interface;

class PacketReaderFactory {
	public:
		PacketReaderFactory() { }
		PacketReader *newPacketReader(Chronicle *supervisor, 
			Interface *interface, bool basicPipeline)
		{
			return new PacketReader(supervisor, interface, basicPipeline);
		}
};

#endif	//PACKET_READER_FACTORY_H

