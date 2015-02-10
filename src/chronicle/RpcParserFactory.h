// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef RPC_PARSER_FACTORY_H
#define RPC_PARSER_FACTORY_H

#include "RpcParser.h"

class ChronicleSource;

class RpcParserFactory {
	public:
		RpcParserFactory() { }
		RpcParser *newRpcParser(ChronicleSource *source, unsigned pipelineId)
		{
			return new RpcParser(source, pipelineId);
		}
};

#endif	//RPC_PARSER_FACTORY_H

