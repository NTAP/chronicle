// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAP_WRITER_FACTORY_H
#define PCAP_WRITER_FACTORY_H

#include "PcapWriter.h"

class ChronicleSource;

class PcapWriterFactory {
	public:
		PcapWriterFactory() { }
		PcapWriter *newPcapWriter(ChronicleSource *source, unsigned pipelineId,
			int snapLen)
		{
			return new PcapWriter(source, pipelineId, snapLen);
		}
};

#endif	//PCAP_WRITER_FACTORY_H

