// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include <exception>
#include "ChronicleProcessRequest.h"
#include "PcapPacketBufferPool.h"

PcapPacketBufferPool *PcapPacketBufferPool::_bufPoolInstance = NULL;
PthreadMutex PcapPacketBufferPool::_bufPoolMutex;

PcapPacketBufferPool::PcapPacketBufferPool(unsigned standardPktPoolSize, 
		unsigned jumboPktPoolSize)
	: PacketBufferPool(standardPktPoolSize, jumboPktPoolSize), 
	_numPkts(standardPktPoolSize + jumboPktPoolSize) 
{
	assert(_bufPoolInstance == NULL && "Error: The constructor called twice "
		"for the singleton PcapPacketBufferPool!");
	_numRegisteredUsers = 0;
	_pktBuffersStandard = NULL;
	_pktBuffersJumbo = NULL;

	if (_pktDescriptors && _pktDescFreeListStandard 
			&& _pktDescFreeListJumbo) {
		try {
			_pktBuffersStandard = new PcapPacketBuffer[standardPktPoolSize];
			_pktBuffersJumbo = new PcapPacketBufferJumbo[jumboPktPoolSize];
		} catch (std::bad_alloc& ba) {
			throw PacketBufPoolException("Could not allocate packet buffers! "
				"Buffer pool is too large!");
		}
	} else if (!_pktDescriptors) {
		throw PacketBufPoolException("Could not allocate packet buffer descriptors! "
			"Buffer pool is too large!");
	} else if (!_pktDescFreeListStandard || !_pktDescFreeListJumbo) {
		throw PacketBufPoolException("Could not allocate packet "
			"buffer descriptors free list! Buffer pool is too large!");
	}
	
	unsigned i;
	for(i = 0; i < _numPkts; i++) {
		_pktDescriptors[i].packetBufferIndex = i;
		if (i < standardPktPoolSize) {
			_pktDescriptors[i].packetBuffer = 
				_pktBuffersStandard[i].getEthFrameAddress();
			_pktDescFreeListStandard->enqueue(&_pktDescriptors[i]);
		} else {
			_pktDescriptors[i].packetBuffer = 
				_pktBuffersJumbo[i - standardPktPoolSize].getEthFrameAddress();
			_pktDescFreeListJumbo->enqueue(&_pktDescriptors[i]);
		}
	}
}

PcapPacketBufferPool::~PcapPacketBufferPool()
{
	if (_pktBuffersStandard) {
		delete[] _pktBuffersStandard;	
		_pktBuffersStandard = NULL;
	}
	if (_pktBuffersJumbo) {
		delete[] _pktBuffersJumbo;	
		_pktBuffersJumbo = NULL;
	}
	_bufPoolInstance = NULL;
}

PcapPacketBufferPool *
PcapPacketBufferPool::getBufferPoolInstance()
{
	return _bufPoolInstance;
}

PcapPacketBufferPool *
PcapPacketBufferPool::registerBufferPool(unsigned standardPktPoolSize,
	unsigned jumboPktPoolSize)
{
	if (_bufPoolInstance == NULL) {
		_bufPoolMutex.lock();
		if (_bufPoolInstance == NULL) {
			try {
				_bufPoolInstance = new PcapPacketBufferPool(
					standardPktPoolSize, jumboPktPoolSize);
			} catch (PacketBufPoolException &exception) {
				std::cerr << FONT_RED << "PcapPacketBufferPool::registerBufferPool: "
					<< exception.getErrorMsg() << FONT_DEFAULT << std::endl;
				_bufPoolInstance = NULL;
			}
		}		
		_bufPoolMutex.unlock();
	}
	if (_bufPoolInstance) {
		_bufPoolInstance->incrementRegisteredUsers();
	}
	return _bufPoolInstance;
}

void 
PcapPacketBufferPool::incrementRegisteredUsers()
{
	__sync_add_and_fetch(&_numRegisteredUsers, 1);
}

bool
PcapPacketBufferPool::unregisterBufferPool()
{
	return (0 == __sync_sub_and_fetch(&_numRegisteredUsers, 1));
}

