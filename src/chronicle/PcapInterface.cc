// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "PcapInterface.h"
#include "ChronicleProcess.h"
#include "PacketReader.h"
#include "Lock.h"
#include "NetworkHeaderParser.h"

void callback(u_char *userArg, const struct pcap_pkthdr *h, 
	const u_char *data);
unsigned int bitRange(unsigned int num, unsigned int start, unsigned int end);

PcapInterface::PcapInterface(const char *name, unsigned batchSize, 
	char *filter, bool toCopy, int snapLen) 
	: Interface(name, batchSize, toCopy, snapLen), _filter(filter)
{
	_handle = NULL;	
	_bufPool = NULL;
}

PcapInterface::~PcapInterface()
{
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;
}

void PcapInterface::close()
{
	if(_handle)
		pcap_close(_handle);
	_handle = NULL;
	_reader = NULL;
}

void PcapInterface::printError(std::string operation)
{
	std::cout << FONT_RED << "PcapInterface::printError: [" << _name << " "
		<< operation << "] " << _errBuf << FONT_DEFAULT << std::endl;
}

int PcapInterface::setFilter(bpf_u_int32 networkNum)
{
	// use the lock for non-thread-safe libpcap routines
	static PthreadMutex _pcap_lock;
	_pcap_lock.lock();
	if (pcap_compile(_handle, &_filterProg, _filter, 0, networkNum) == -1) {
		strcpy(_errBuf, pcap_geterr(_handle));
		_pcap_lock.unlock();
		return IF_ERR;
	}
	if (pcap_setfilter(_handle, &_filterProg) == -1) {
		strcpy(_errBuf, pcap_geterr(_handle));
		_pcap_lock.unlock();
		return IF_ERR;
	}
	_pcap_lock.unlock();
	return 0;
}

int PcapInterface::pcapDispatch(u_char *userArg)
{
	return pcap_dispatch(_handle, _batchSize, callback, userArg);
}

void callback(u_char *userArg, const struct pcap_pkthdr *h, const u_char *data)
{
	static bool warningIssued = false;
	PacketDescriptor *pktDesc = NULL;
	PcapInterface *interface = reinterpret_cast <PcapInterface *> (userArg);
	++interface->_chronPktsRead;
    interface->_chronBytesRead += h->caplen;
	// obtain and initialize a free packet buffer descriptor
	PcapPacketBufferPool *_bufPool = 
		PcapPacketBufferPool::getBufferPoolInstance();
	assert(_bufPool != NULL);
	while (pktDesc == NULL) {
		pktDesc = _bufPool->getPacketDescriptor(h->caplen);
		if (pktDesc == NULL && interface->_type == "PcapOfflineInterface") {
			if (h->len > MAX_ETH_FRAME_SIZE_STAND 
					&& JUMBO_PKTS_IN_BUFFER_POOL == 0) {
				std::cout << "ignoring jumbo packets\n";
				return ;
			} else
				usleep(10000);
		} else if (pktDesc == NULL) { // PcapOnlineInterface
			#if CHRON_DEBUG(CHRONICLE_DEBUG_NETWORK)
			std::cout << "[" << interface->getName() << "] " << "no available "
				"space in the packet buffer pool! skipping this packet.\n";
			#endif
			++interface->_chronPktsDropped;
			return ;
		}
	}

	// setting up the packet descriptor (partially)
	pktDesc->interface = interface;
	pktDesc->pcapHeader.ts.tv_sec = h->ts.tv_sec;
	pktDesc->pcapHeader.ts.tv_usec = h->ts.tv_usec;
	pktDesc->pcapHeader.len = h->len;
	pktDesc->pcapHeader.caplen = MIN(h->caplen,
		MAX_ETH_FRAME_SIZE_JUMBO);
	pktDesc->next = NULL;
	if (!warningIssued && pktDesc->pcapHeader.caplen 
			> MAX_ETH_FRAME_SIZE_JUMBO) {
		std::cout << "[" << interface->getName() << "] " 
			<< "WARNING: large packet (> " << MAX_ETH_FRAME_SIZE_JUMBO 
			<< ") - see README\n";
			warningIssued = true;
	}
	memcpy(pktDesc->packetBuffer, data, pktDesc->pcapHeader.caplen);
	PacketReader *reader = interface->getPacketReader();
	reader->_networkHdrParser->processRequest(reader, pktDesc);	
}

PcapOfflineInterface::PcapOfflineInterface(const char *name, unsigned batchSize, 
	char *filter, bool toCopy, int snapLen) 
	: PcapInterface(name, batchSize, filter, toCopy, snapLen)
{
	_type = "PcapOfflineInterface";
}

PcapOfflineInterface::~PcapOfflineInterface()
{

}

int PcapOfflineInterface::open()
{
	std::cout << FONT_GREEN << "[" << _name << "]" << " opened" 
		<< FONT_DEFAULT << std::endl;
	_handle = pcap_open_offline(_name, _errBuf);	
	if (_handle == NULL) 
		return IF_ERR;
	if (_filter &&  setFilter(0) == IF_ERR) 
		return IF_ERR;

	// register the interface/reader with the buffer pool
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL)
		return IF_ERR;
	// setting up poll
	memset(_fds, 0, sizeof(_fds));
	_fds[0].fd = _reader->getFDQueueFd();
	_fds[0].events = (POLLIN);
	return 0;
}

Interface::InterfaceStatus PcapOfflineInterface::read()
{
	int ret;
	if (_handle == NULL) {
		strcpy(_errBuf, "invalid handle, interface must have been closed already!");
		_status = IF_ERR;
		return _status;
	}
retry:
	errno = 0;
	if ((ret = poll(_fds, sizeof(_fds)/sizeof(struct pollfd), 0)) == 0) 
		;
	else if (ret < 0) {
		/* for gprof //TODO: DEL
		sprintf(errBuf, "poll: %s", strerror(errno));
		return IF_ERR;		
		*/
		if (ret == -1 && errno == EINTR)
			goto retry;
		perror("poll by PcapOnlineInterface");
		_status = IF_ERR;
		return _status;
	} else {
		if ((_fds[0].revents & (POLLIN)))
			_reader->handleFDQueue();
	}
	if (_status == IF_ACTIVE || _status == IF_IDLE) {
		ret = pcapDispatch(reinterpret_cast<u_char *> (this));	//XXX
		switch (ret) {
			case 0:
				_status = IF_DONE;
				break;
			case -1:
				strcpy(_errBuf, pcap_geterr(_handle));
				_status = IF_ERR;
				break;
			case -2:	/* due to pcap_breakloop */
				//status = IF_BREAK;
				break;
			default:
				if (ret == static_cast<int> (_batchSize))
					_filledBatches++;
				_status = IF_ACTIVE;
		}
	}
	return _status;
}

PcapOnlineInterface::PcapOnlineInterface(const char *name, unsigned batchSize, 
	char *filter, bool toCopy, int snapLen, int timeOut) 
	: PcapInterface(name, batchSize, filter, toCopy, snapLen), _timeOut(timeOut)
{
	PcapOnlineInterface::getNicInfo(name, &_networkNum, &_networkMask, 
		&_errBuf[0]);
	/*
	if (networkNum || networkMask) {
		printAddress(static_cast<unsigned> (networkNum));
		printAddress(static_cast<unsigned> (networkMask));
	}
	*/
	_type = "PcapOnlineInterface";
}

PcapOnlineInterface::~PcapOnlineInterface()
{

}

int PcapOnlineInterface::open()
{
	std::cout << FONT_GREEN << "[" << _name << "]" << " opened"
		<< FONT_DEFAULT << std::endl;
	_handle = pcap_open_live(_name, _snapLen, 1, _timeOut, _errBuf);
	if (_handle == NULL)
		return IF_ERR;
	if (_filter &&  setFilter(_networkNum) == IF_ERR) {
		return IF_ERR;
	}
	if (pcap_setnonblock(_handle, 1, _errBuf) == -1)
		return IF_ERR;
	if ((_fd = pcap_get_selectable_fd(_handle)) == -1) {
		strcpy(_errBuf, "PcapOnlineInterface::open: non-selectable file descriptor\n"); 
		return IF_ERR;
	}
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

	return 0;
}

Interface::InterfaceStatus PcapOnlineInterface::read()
{
	int ret;
	if (_handle == NULL) {
		strcpy(_errBuf, "invalid handle, interface must have been closed already!");
		_status = IF_ERR;
		return _status;
	}
	errno = 0;
	if ((ret = poll(_fds, sizeof(_fds)/sizeof(struct pollfd), -1)) == 0) {
		_status = IF_IDLE;
		return _status;
	} else if (ret < 0) {
		/* for gprof //TODO: DEL
		sprintf(errBuf, "poll: %s", strerror(errno));
		return IF_ERR;		
		*/
		perror("poll by PcapOnlineInterface");
		_status = IF_ERR;
		return _status;
	}
	if ((_fds[1].revents & (POLLIN))) 
		_reader->handleFDQueue();
	if ((_status == IF_ACTIVE || _status == IF_IDLE) && 
			(_fds[0].revents & (POLLIN))) {
		getNicStats();
		ret = pcapDispatch(reinterpret_cast<u_char *> (this));
		switch (ret) {
			case -1:
				strcpy(_errBuf, pcap_geterr(_handle));
				_status = IF_ERR;
				break;
			case -2:	/* due to pcap_breakloop */
				//status = IF_BREAK;
				break;
			case 0:
				_status = IF_IDLE;
				break;
			default:
				if (ret == static_cast<int> (_batchSize))
					_filledBatches++;
				_status = IF_ACTIVE;
		}
	}
	return _status;
}

void PcapOnlineInterface::getNicStats()
{
	struct pcap_stat nicStat;
	if (_handle == NULL || pcap_stats(_handle, &nicStat) == -1) {
		_nicPktsRead = _nicPktsDropped = 0;
		return ;
	}
	_nicPktsRead = nicStat.ps_recv;
	_nicPktsDropped = nicStat.ps_drop;	
}

void PcapOnlineInterface::getNicInfo(const char *name, bpf_u_int32 *networkNum,
	bpf_u_int32 *networkMask, char *errBuf)
{
	if (pcap_lookupnet(name, networkNum, networkMask, errBuf) == -1) {
		/* not very useful as contrary to the documentation networkNum 
		 * is really ip addr & networkMask; however, it is still used
		 * by the filter (only for broadcast addresses). */
		*networkNum = 0;	
		*networkMask = 0;
	}
}

char *PcapOnlineInterface::getDefaultNic()
{
	char *ret;
	char errbuf[PCAP_ERRBUF_SIZE];
	ret = pcap_lookupdev(errbuf);
	if (ret == NULL)
		std::cerr << "PcapOnlineInterface::getDefaultNic: " 
			<< errbuf << std::endl;			
	return ret;
}

int PcapOnlineInterface::getAllNics(std::list<char *>& nics)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *dev;
	int ret;
	if ((ret = pcap_findalldevs(&dev, errbuf)) == -1) {
		std::cout << " PcapOnlineInterface::getAllNics: " 
			<< errbuf << std::endl;
		return -1;
	}
	while (dev) {
		//XXX: find all the eth* devs
		if(!strncmp(dev->name, "eth", 3))
			nics.push_back(dev->name);
		dev = dev->next;
	}
	return 0;
}

void PcapOnlineInterface::printAddress(unsigned addr)
{
	std::cout << bitRange(addr, 0, 7) << "."
		<< bitRange(addr, 8, 15) << "."
		<< bitRange(addr, 16, 23) << "."
		<< bitRange(addr, 24, 31) << std::endl;
}

unsigned int 
bitRange(unsigned int num, unsigned int start, unsigned int end)
{
	unsigned int shiftLeft, shiftRight;

	if (start > 31 || end < start || end > 31)
		fprintf(stderr, "getBitRange: invalid range\n");
	shiftLeft = sizeof(unsigned int)*8 - 1 - end;
	shiftRight = start + shiftLeft;

	return (num << shiftLeft) >> shiftRight; 
}
