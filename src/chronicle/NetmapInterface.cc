// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#include <net/if.h>
#include <stdio.h>
#include <errno.h>
#include "NetmapInterface.h"
#include "ChronicleProcess.h"
#include "PacketReader.h"
#include "Lock.h"
#include "NetworkHeaderParser.h"
#include "ChroniclePipeline.h"

#define unlikely(x)						__builtin_expect(!!(x), 0)
#define NETMAP_RING_NEXT(ring, cur)		\
	(unlikely(cur + 1 == ring->num_slots) ? 0 : cur + 1)

// functions to avoid naming conflicts between the class and POSIX routines
int _open()
{
	int fd = open("/dev/netmap", O_RDWR);
	return fd;
}

void _close(int fd)
{
	close(fd);
}

void NetmapInterface::processPackets(uint16_t ringId, struct netmap_ring *ring)
{
	uint32_t cur, rx;
	static bool warningIssued = false;
	PacketDescriptor *pktDesc = NULL, *firstPktDesc = NULL, *lastPktDesc = NULL;
	PcapPacketBufferPool *_bufPool = 
		PcapPacketBufferPool::getBufferPoolInstance();
	assert(_bufPool != NULL);
	cur = ring->cur;
	
	for (rx = 0; rx < _batchSize && !nm_ring_empty(ring); rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *packet = NETMAP_BUF(ring, slot->buf_idx);
		_chronPktsRead++;
		
		#if CHRON_DEBUG(CHRONICLE_DEBUG_PKTLOSS) //for the paper experiments
		if (_curInterval < MAX_INTERVAL &&
				ring->ts.tv_sec > _intervalStartTime[_curInterval+1]) {
			_curInterval ++;
			if (_intervalLossRate[_curInterval] != 0) {
				_dropInterval = 1000 / _intervalLossRate[_curInterval];
				_nextDrop = _dropInterval;
				_curCount = 0;			
			} else {
				_nextDrop = 0;
			}
			printf("[%p] time:%ld loss:%u _nextDrop:%u\n", this,
				_intervalStartTime[_curInterval],
				_intervalLossRate[_curInterval], _nextDrop);
		}			
		if (slot->len > 100 && _intervalLossRate[_curInterval] > 0 && 
				_curCount++ == _nextDrop) {
			_nextDrop += _dropInterval;
			if (_curCount == 1000) {
				_curCount = 0;
				_nextDrop = _dropInterval;
			}
			cur = NETMAP_RING_NEXT(ring, cur);
			ring->head = ring->cur = cur;
			/*
			printf("[%p] interval:%u dropped %luth packet\n", this, 
				_curInterval, _chronPktsRead);
			*/
			continue;
		}
		#endif

		pktDesc = _bufPool->getPacketDescriptor(slot->len);
		if (pktDesc == NULL) {
			/*
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			std::cout << "[" << getName() << "] " << "no available "
				"space in the packet buffer pool!\n";
			#endif
			*/
			++_chronPktsDropped;
			if (_chronPktsDropped > 100000)
				_status = IF_FULL;
			// break ; // if we want to retry instead of drop
			cur = NETMAP_RING_NEXT(ring, cur);
			ring->head = ring->cur = cur;
			continue ;
		}

		// setting up the packet descriptor (partially)
		pktDesc->interface = this;
		pktDesc->ringId = ringId;
		pktDesc->pcapHeader.ts = ring->ts;
		pktDesc->pcapHeader.len = slot->len;
		pktDesc->visitCount = pktDesc->flag = 0;

		#if JUMBO_PKTS_IN_BUFFER_POOL > 0
		if (!warningIssued && pktDesc->pcapHeader.len 
				> MAX_ETH_FRAME_SIZE_JUMBO) {
			std::cout << "[" << getName() << "] " 
				<< "WARNING: large packet (> " << MAX_ETH_FRAME_SIZE_JUMBO 
				<< ") - see README\n";
			warningIssued = true;
		}
		pktDesc->pcapHeader.caplen = MIN(pktDesc->pcapHeader.len,
			MAX_ETH_FRAME_SIZE_JUMBO);
		#else
		if (!warningIssued && pktDesc->pcapHeader.len 
				> MAX_ETH_FRAME_SIZE_STAND) {
			std::cout << "[" << getName() << "] " 
				<< "WARNING: large packet (> " << MAX_ETH_FRAME_SIZE_STAND 
				<< ") - see README\n";
			warningIssued = true;
		}
		pktDesc->pcapHeader.caplen = MIN(pktDesc->pcapHeader.len,
			MAX_ETH_FRAME_SIZE_STAND);
		#endif
		_chronBytesRead += pktDesc->pcapHeader.caplen;
		if (pktDesc->pcapHeader.caplen < pktDesc->pcapHeader.len)
			pktDesc->flag = PACKET_TRUNCATED;
		memcpy(pktDesc->packetBuffer, packet, pktDesc->pcapHeader.caplen);

		// creating a list of packets
		if (firstPktDesc == NULL)
			firstPktDesc = lastPktDesc = pktDesc;
		else { 
			lastPktDesc->next = pktDesc;
			lastPktDesc = pktDesc;
		}
		cur = NETMAP_RING_NEXT(ring, cur);
		ring->head = ring->cur = cur;
	}

	if (rx >= _batchSize - 2)
		_filledBatches++;
	if (rx > _maxBatchSizeRead)
		_maxBatchSizeRead = rx;
	
	// passing the list of packets
	if (firstPktDesc != NULL) {
		lastPktDesc->next = NULL;
		PacketReader *reader = getPacketReader();
		reader->_networkHdrParser->processRequest(reader, firstPktDesc);
	}
}

NetmapInterface::NetmapInterface(const char *name, unsigned batchSize, 
	bool toCopy, int snapLen, int timeOut) 
	: Interface(name, batchSize, toCopy, snapLen), _timeOut(timeOut)
{
	_type = "NetmapInterface";
	_bufAddr = NULL;
	_bufPool = NULL;
	_fd = 0;
	bzero(_interface, sizeof(_interface));
	#if CHRON_DEBUG(CHRONICLE_DEBUG_PKTLOSS)
	int i;
	_curCount = _curInterval = 0;
	/*
	_dropInterval = 1000 / LOSS_PER_1000_PKTS;	
	_nextDrop = _dropInterval;
	*/
	for (i = 0; i < MAX_INTERVAL; i += 2)
		_intervalLossRate[i] = 0;
	for (i = 1; i < MAX_INTERVAL; i += 2)
		_intervalLossRate[i] = 5 * (i + 1) / 2;
		//_intervalLossRate[i] = 5 * (i + 1);
	for (i = 0; i < MAX_INTERVAL; i += 2)
		_intervalDuration[i] = 2 * 60;
	for (i = 1; i < MAX_INTERVAL; i += 2)
		//_intervalDuration[i] = 30;
		_intervalDuration[i] = 60;
	_intervalStartTime[0] = time(0);
	for (i = 1; i < MAX_INTERVAL; i++)
		_intervalStartTime[i] = _intervalStartTime[i-1] + _intervalDuration[i-1];
	#endif
}

NetmapInterface::~NetmapInterface()
{
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;
}

int NetmapInterface::open()
{
	struct ifreq ifReq;	
	int socketFd;
	char *addr;
	uint16_t nr_ringid = 0;
	uint32_t nr_flags = 0;
	struct netmap_ring *rxRing;
	
	// opening the interface in the netmap mode
	_fd = _open();
	if (_fd < 0) {
		strcpy(_errBuf, "Unable to open /dev/netmap. Is netmap module loaded?"
				" Are you running as root?");
		return IF_ERR;
	}
	
	/* // validating the interface name
	if (strncmp(_name, "netmap:", 7)) {
		strcpy(_errBuf, "Interface name needs to start with \"netmap:\".");
		return IF_ERR;
	}
	_name += 7;
	*/
	if ((addr = strchr(const_cast<char *>(_name), '-')) == NULL) {
		// monitoring all the rings
		strcpy(_interface, _name);
		nr_flags = NR_REG_ALL_NIC;
	} else {
		// monitoring a specific ring
		strncpy(_interface, _name, addr - _name);
		nr_flags = NR_REG_ONE_NIC;
		nr_ringid = atoi(addr + 1);
		if (nr_ringid > NETMAP_RING_MASK) {
			strcpy(_errBuf, "Invalid ring ID!");
			return IF_ERR;
		}
		nr_ringid |= (NETMAP_NO_TX_POLL | NETMAP_DO_RX_POLL);

	}
	if (strlen(const_cast<char *>(_interface)) > 
			sizeof(_netmapReq.nr_name)) {
		strcpy(_errBuf, "Interface name is too long!");
		return IF_ERR;
	}
	
	bzero(&_netmapReq, sizeof(_netmapReq));
	_netmapReq.nr_version = NETMAP_API;
	_netmapReq.nr_ringid &= ~NETMAP_RING_MASK;
	_netmapReq.nr_ringid |= nr_ringid;
	_netmapReq.nr_flags = nr_flags;
	strcpy(_netmapReq.nr_name, _interface);
	if (ioctl(_fd, NIOCREGIF, &_netmapReq) == -1) {
		sprintf(_errBuf, "NIOCREGIF failed for %s: %s", _netmapReq.nr_name,
			strerror(errno));
		return IF_ERR;
	}
	_numRxRings = _netmapReq.nr_rx_rings;
	/* //TODO: DEL
	printf("interface:%s ring:%u flag:%u rings:%u\n", 
		_netmapReq.nr_name, _netmapReq.nr_ringid, _netmapReq.nr_flags, _numRxRings);
	*/
	_bufAddr = mmap(0, _netmapReq.nr_memsize, PROT_WRITE | PROT_READ, MAP_SHARED,
		_fd, 0);
	if (_bufAddr == MAP_FAILED) {
		sprintf(_errBuf, "Unable to mmap %d KB!", _netmapReq.nr_memsize >> 10);
		return IF_ERR;
	}
	_netmapIf = NETMAP_IF(_bufAddr, _netmapReq.nr_offset);
	if (nr_flags ==  NR_REG_ALL_NIC) {
		_firstRxRing = 0;
		_lastRxRing = _netmapReq.nr_rx_rings - 1;
	} else {
		_firstRxRing = _lastRxRing = nr_ringid;
	}
	rxRing = NETMAP_RXRING(_netmapIf, _firstRxRing);
	if (rxRing->num_slots < _batchSize) {
		printf("NIC ring has only %u slots. Changing the batch size to %u.\n",
			rxRing->num_slots, rxRing->num_slots);
		_batchSize = rxRing->num_slots;
	}

	// setting the promiscuous flag
	socketFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFd < 0) {
		strcpy(_errBuf, "Cannot get device control socket");
		return IF_ERR;
	}
	bzero(&ifReq, sizeof(ifReq));
	strcpy(ifReq.ifr_name, _netmapReq.nr_name);
	errno = 0;
	if (ioctl(socketFd, SIOCGIFFLAGS, &ifReq) == -1) {
		sprintf(_errBuf, "ioctl: %s", strerror(errno));
		return IF_ERR;
	}
	ifReq.ifr_flags |= IFF_PROMISC;
	errno = 0;
	if (ioctl(socketFd, SIOCSIFFLAGS, &ifReq) == -1) {
		sprintf(_errBuf, "ioctl: %s", strerror(errno));
		return IF_ERR;
	}
	_close(socketFd);

	// setting up poll
	memset(_fds, 0, sizeof(_fds));
	_fds[0].fd = _fd;
	_fds[0].events = (POLLIN);
	_fds[1].fd = _reader->getFDQueueFd();
	_fds[1].events = (POLLIN);

	// register the interface/reader with the buffer pool
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		return IF_ERR;

	std::cout << FONT_GREEN << "[" << _name << "]" << " opened with " 
		<< _numRxRings << " RX ring(s) and " << (_netmapReq.nr_memsize >> 10)
		<< "KB of memory" << FONT_DEFAULT << std::endl;	
	return 0;
}

Interface::InterfaceStatus NetmapInterface::read()
{
	int ret;
	struct netmap_ring *rxRing;
	errno = 0;
retry:
	#if PACKET_READER_PRIORITIZED 
	// Timeout value of 0 minimizes packet loss but saturates CPU on systems
	// with very few cores (e.g., the shuttle platform).
	if ((ret = poll(_fds, sizeof(_fds)/sizeof(struct pollfd), 0)) == 0) {
	#else
	if ((ret = poll(_fds, sizeof(_fds)/sizeof(struct pollfd), -1)) == 0) {
	#endif
		_status = IF_IDLE;
		return _status;
	} else if (ret < 0) {
		/* for gprof //TODO: DEL
		sprintf(errBuf, "poll: %s", strerror(errno));
		return IF_ERR;		
		*/
		if (ret == -1 && errno == EINTR)
			goto retry;
		perror("poll by NetmapInterface");
		_status = IF_ERR;
		return _status;
	}
	if ((_status == IF_ACTIVE || _status == IF_IDLE) && (_fds[0].revents & (POLLIN))) {
		for (uint16_t i = _firstRxRing; i <= _lastRxRing; i++) {
			rxRing = NETMAP_RXRING(_netmapIf, i);
			if (nm_ring_empty(rxRing))
				continue;
			/* printf(" NetmapInterface::read [BEFORE] RX%u %p h %d c %d t %d\n",
				i, rxRing, rxRing->head, rxRing->cur, rxRing->tail);*/
			//printPackets(rxRing); 
			processPackets(i, rxRing);
			/* printf(" NetmapInterface::read [AFTER] RX%u %p h %d c %d t %d\n\n", 
				i, rxRing, rxRing->head, rxRing->cur, rxRing->tail);*/
		}
		if (_status != IF_FULL)
			_status = IF_ACTIVE;
	}
	if ((_fds[1].revents & (POLLIN))) 
		_reader->handleFDQueue();
	return _status;
}

void NetmapInterface::close()
{
	if (_bufAddr)
		munmap(_bufAddr, _netmapReq.nr_memsize);
	if (_fd > 0) {
		//ioctl(_fd, NIOCUNREGIF, &_netmapReq);
		_close(_fd);	
	}
	_reader = NULL;
}

void NetmapInterface::printError(std::string operation)
{
	std::cout <<  FONT_RED << "NetmapInterface::printError: [" << _name << " "
		<< operation << "] " << _errBuf << FONT_DEFAULT << std::endl;
}

uint32_t NetmapInterface::printPackets(struct netmap_ring *ring)
{
	uint32_t cur, rx;
	unsigned batchSize;

	cur = ring->cur;
	if (ring->tail - ring->cur < _batchSize)
		batchSize = ring->tail - ring->cur;
	else
		batchSize = _batchSize;
	for (rx = 0; rx < batchSize; rx++) {
		_chronPktsRead++;
		struct netmap_slot *slot = &ring->slot[cur];
		uint16_t len;
		uint32_t i, j, tmp;
		char *p = NETMAP_BUF(ring, slot->buf_idx);
		time_t statTime = ring->ts.tv_sec;
		struct tm *statTm = localtime(&statTime);
		char buf1[30], buf2[30];
		strftime(buf1, sizeof(buf1), "%D %T", statTm);
		snprintf(buf2, sizeof(buf2), "%s:%06u", buf1, (unsigned) ring->ts.tv_usec);
		len = slot->len;
		printf("[%lu] %s read %u/%d pkt:%p %uB\n", _chronPktsRead, buf2, rx+1,
			batchSize, p, len);
		for (i=0; i<len; i+=4) {
			unsigned char *up = (unsigned char *) p;
			tmp = 0;
			for (j=0; j<4 && j+i<len; j++) {
				tmp <<= 8;
				tmp |= up[i+j];
			}
			switch (j) {
				case 1:
					printf("%02x", tmp);
					break;
				case 2:
					printf("%04x", tmp);
					break;
				case 3:
					printf("%06x", tmp);
					break;
				case 4:
					printf("%08x", tmp);
					break;
			}
		}
		printf("\n");
		cur = NETMAP_RING_NEXT(ring, cur); 
		ring->head = ring->cur = cur;
	}

	return (rx);
}
