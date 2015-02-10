// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <sys/time.h>
#include <string>

class PacketReader;

/**
 * An abstract interface class
 */
class Interface {
	public:
		/// Interface states
		typedef enum status {
			IF_ERR = -1,
			IF_ACTIVE = -2,
			IF_IDLE = -3,
			IF_TERMINATING = -4,
			IF_DONE = -5,
			IF_FULL = -6
		} InterfaceStatus;
		Interface(const char *name, uint32_t batchSize, bool toCopy, int snapLen)
			: _name(name), _batchSize(batchSize), _toCopy(toCopy), _snapLen(snapLen)
		{ 
			_filledBatches = _maxBatchSizeRead = _chronPktsRead = 
				_chronPktsDropped = _nicPktsRead = _nicPktsDropped =
                _chronBytesRead = 0; 
		}
		virtual ~Interface() { }
		virtual int open() = 0;
		virtual InterfaceStatus read() = 0;
		virtual void close() = 0;
		virtual void printError(std::string operation) = 0;
		virtual char *getFilter() = 0;
		virtual bool getToCopy() = 0;
		virtual void getNicStats() = 0;
		const char *getName() { return _name; }
		uint32_t getBatchSize() { return _batchSize; }
		uint32_t getFilledBatches() { return _filledBatches; }
		uint32_t getMaxBatchSizeRead() { return _maxBatchSizeRead; }
		uint64_t getPacketsRead() { return _chronPktsRead; }
		uint64_t getPacketsDropped() { return _chronPktsDropped; }
		uint64_t getBytesRead() { return _chronBytesRead; }
		uint64_t getNicPacketsRead() { return _nicPktsRead; }
		uint64_t getNicPacketsDropped() { return _nicPktsDropped; }
		int getSnapLen() { return _snapLen; }
		void setPacketReader(PacketReader *r) { _reader = r; }
		PacketReader *getPacketReader() { return _reader; }
		InterfaceStatus getStatus() { return _status; }

		/// name of the interface
		const char *_name;
		/// interface (selectable) fd
		int _fd;
		/// type of the interface 
		std::string _type;
		/// read batch size
		uint32_t _batchSize;
		/// the number of times that a full batch is read
		uint32_t _filledBatches;
		/// the maximum batch size read
		uint32_t _maxBatchSizeRead;
		/// num packets read (after filtering)
		uint64_t _chronPktsRead;
		/// num packets dropped
		uint64_t _chronPktsDropped;
		/// Bytes received by this interface
		uint64_t _chronBytesRead;
		/// num packets read by driver/kernel/libpcap in kernel-space
		uint64_t _nicPktsRead;		
		/// num packets dropped by driver/kernel/libpcap in kernel-space
		uint64_t _nicPktsDropped;
		/// whether to copy packet header/payload
		bool _toCopy;
		/// snapshot length
		int _snapLen;		
		/// the exit status of the interface
		InterfaceStatus _status;
		/// pointer to the corresponding reader 
		/// (used by the pcap/netmap callback that reads packets)
		PacketReader *_reader;
};

/**
 * The interface stat class
 */
class InterfaceStat {
	public:
		InterfaceStat(Interface *i) 
		{
			gettimeofday(&_tv, NULL);
			_name = i->getName();
			_type = i->_type;
			_batchSize = i->getBatchSize();
			_filledBatches = i->getFilledBatches();
			_maxBatchSizeRead = i->getMaxBatchSizeRead();
			_chronPktsRead = i->getPacketsRead();
			_chronPktsDropped = i->getPacketsDropped();
			_nicPktsRead = i->getNicPacketsRead();
			_nicPktsDropped = i->getNicPacketsDropped();
			_status = i->_status;
		}
		const char *getName() { return _name; }
		const std::string getType() { return _type; }
		uint32_t getBatchSize() { return _batchSize; }
		uint32_t getFilledBatches() { return _filledBatches; }
		uint32_t getMaxBatchSizeRead() { return _maxBatchSizeRead; } 
		uint64_t getPacketsRead() { return _chronPktsRead; }
		uint64_t getPacketsDropped() { return _chronPktsDropped; }
		uint64_t getNicPacketsRead() { return _nicPktsRead; }
		uint64_t getNicPacketsDropped() { return _nicPktsDropped; }
		Interface::InterfaceStatus getStatus() { return _status; }
		struct timeval getTime() { return _tv; }

	private:
		/// name of the interface
		const char *_name;
		/// type of the interface 
		std::string _type;
		/// read batch size
		uint32_t _batchSize;
		/// the number of times that a full batch is read
		uint32_t _filledBatches;
		/// the maximum batch size read
		uint32_t _maxBatchSizeRead;
		/// num packets read (after filtering) by Chronicle
		uint64_t _chronPktsRead;
		/// num packets dropped by Chronicle
		uint64_t _chronPktsDropped;
		/// num packets read by driver/kernel/libpcap in kernel-space
		uint64_t _nicPktsRead;		
		/// num packets dropped by driver/kernel/libpcap in kernel-space
		uint64_t _nicPktsDropped;
		/// the exit status of the interface
		Interface::InterfaceStatus _status;
		/// the time the stat was gathered
		struct timeval _tv;
};

#endif //INTERFACE_H

