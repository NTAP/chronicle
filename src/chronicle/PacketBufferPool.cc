// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include "PacketBufferPool.h"
#include "ChronicleProcessRequest.h"

PacketBufferPool::PacketBufferPool(unsigned standardPktPoolSize, 
			unsigned jumboPktPoolSize) 
	: _standardPktPoolSize(standardPktPoolSize),
	_jumboPktPoolSize(jumboPktPoolSize)
{
	_pktDescriptors = NULL;
	_pktDescFreeListStandard = _pktDescFreeListJumbo = NULL;
	_pktDescriptors = new (std::nothrow) 
		PacketDescriptor[_standardPktPoolSize + _jumboPktPoolSize];
	if(_pktDescriptors) {
		_pktDescFreeListStandard = 
			new (std::nothrow) TSQueue<PacketDescriptor>;
		_pktDescFreeListJumbo = 
			new (std::nothrow) TSQueue<PacketDescriptor>;
	}
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
	_allocatedBufsStandard = _maxAllocatedBufsStandard = 
		_timesNoFreeBufsStandard = 0;
	_allocatedBufsJumbo = _maxAllocatedBufsJumbo = _timesNoFreeBufsJumbo = 0;
	#endif
}

PacketBufferPool::~PacketBufferPool()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
	printf("PacketBufferPool::~PacketBufferPool: "
		"allocdBufsStand:%lu maxAllocdBufsStand:%lu noFreeBufsStand:%lu "
		"allocdBufsJumbo:%lu maxAllocdBufsJumbo:%lu noFreeBufsJumbo:%lu\n", 
		_allocatedBufsStandard.load(std::memory_order_acquire),
		_maxAllocatedBufsStandard.load(std::memory_order_acquire),
		_timesNoFreeBufsStandard.load(std::memory_order_acquire),
		_allocatedBufsJumbo.load(std::memory_order_acquire),
		_maxAllocatedBufsJumbo.load(std::memory_order_acquire),
		_timesNoFreeBufsJumbo.load(std::memory_order_acquire));
	unsigned j = 0, k = 0;
	for(unsigned i = 0; 
			_pktDescriptors && i < _standardPktPoolSize + _jumboPktPoolSize;
			i++) {
		if (_pktDescriptors[i].refCount.load() != 0)
			printf("j:%d index:%u refCount:%u\n", 
				++j, i, _pktDescriptors[i].refCount.load());
		if (_pktDescriptors[i].allocated.load() == true) {
			printf("k:%d index:%u refCount:%u\n", 
				++k, i, _pktDescriptors[i].refCount.load());
			_pktDescriptors[i].print();
		}
	}
	#endif
	if (_pktDescriptors)
		delete[] _pktDescriptors;
	if (_pktDescFreeListStandard) 
		delete _pktDescFreeListStandard;
	if (_pktDescFreeListJumbo) 
		delete _pktDescFreeListJumbo;
}

PacketDescriptor *
PacketBufferPool::getPacketDescriptor(unsigned ethFrameSize)
{
	if (ethFrameSize <= MAX_ETH_FRAME_SIZE_STAND) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
		PacketDescriptor *pktDesc = _pktDescFreeListStandard->dequeue();
		if (pktDesc == NULL)
			_timesNoFreeBufsStandard++;
		else {
			_allocatedBufsStandard++;
			if (_allocatedBufsStandard > _maxAllocatedBufsStandard)
				_maxAllocatedBufsStandard = _allocatedBufsStandard.load();
			assert(pktDesc->allocated == false && 
				pktDesc->refCount.load() == 0);
			pktDesc->allocated = true;
			pktDesc->refCount = 1;
		}
		return pktDesc;	
		#else
		PacketDescriptor *pktDesc = _pktDescFreeListStandard->dequeue();
		if (pktDesc != NULL)
			pktDesc->refCount = 1;
		return pktDesc;
		#endif
	} else {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
		PacketDescriptor *pktDesc = _pktDescFreeListJumbo->dequeue();
		if (pktDesc == NULL)
			_timesNoFreeBufsJumbo++;
		else {
			_allocatedBufsJumbo++;
			if (_allocatedBufsJumbo > _maxAllocatedBufsJumbo)
				_maxAllocatedBufsJumbo = _allocatedBufsJumbo.load();
			assert(pktDesc->allocated == false);
			pktDesc->allocated = true;
			pktDesc->refCount = 1;
		}
		return pktDesc;	
		#else
		PacketDescriptor *pktDesc = _pktDescFreeListJumbo->dequeue();
		if (pktDesc != NULL)
			pktDesc->refCount = 1;
		return pktDesc;
		#endif
	}
}

bool 
PacketBufferPool::releasePacketDescriptor(PacketDescriptor *pktDesc)
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
	if (!pktDesc->allocated) { 
		pktDesc->print();
		fflush(stdout);
	}
	assert(pktDesc->allocated == true);
	#endif
	if (pktDesc->refCount.fetch_sub(1) != 1)
		return false;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
	if (pktDesc->packetBufferIndex < _standardPktPoolSize)
		_allocatedBufsStandard--;
	else
		_allocatedBufsJumbo--;
	assert(pktDesc->refCount == 0);
	pktDesc->allocated = false;
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
	pktDesc->lastTouchingProcess = 0;
	pktDesc->pduDesc = NULL;
	#endif
	if (pktDesc->packetBufferIndex < _standardPktPoolSize)
		_pktDescFreeListStandard->enqueue(pktDesc);
	else
		_pktDescFreeListJumbo->enqueue(pktDesc);
	return true;
}

void 
PacketBufferPool::print(unsigned index)
{
	if (index >= _standardPktPoolSize + _jumboPktPoolSize) {
		printf("PacketBufferPool::print: index is too large\n");
		return ;
	}
	_pktDescriptors[index].print();
}

#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
void
PacketBufferPool::getBufPoolStats(uint64_t &allocatedBufsStandard, 
	uint64_t &maxAllocdBufsStandard, uint64_t &timesNoFreeBufsStandard,
	uint64_t &allocatedBufsJumbo, uint64_t &maxAllocatedBufsJumbo,
	uint64_t &timesNoFreeBufsJumbo)
{
	allocatedBufsStandard = _allocatedBufsStandard.load();
	maxAllocdBufsStandard = _maxAllocatedBufsStandard.load();
	timesNoFreeBufsStandard = _timesNoFreeBufsStandard.load();
	allocatedBufsJumbo = _allocatedBufsJumbo.load();
	maxAllocatedBufsJumbo = _maxAllocatedBufsJumbo.load();
	timesNoFreeBufsJumbo = _timesNoFreeBufsJumbo.load();
}
#endif
