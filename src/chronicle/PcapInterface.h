// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef PCAP_INTERFACE_H
#define PCAP_INTERFACE_H

#include <poll.h>
#include <pcap.h>
#include <list>
#include "Interface.h"
#include "ChronicleConfig.h"
#include "PcapPacketBufferPool.h"

/**
 * An abstract class for pcap interfaces (file or NIC)
 */
class PcapInterface : public Interface {
	public:
		PcapInterface(const char *name, uint32_t batchSize, char *filter,
			bool toCopy, int snapLen);
		virtual ~PcapInterface();
		virtual int open() = 0;
		virtual InterfaceStatus read() = 0;
		void close();
		/**
		 * prints the error for this pcap interface
		 * @param[in] operation The operation that caused error (e.g., open,
		 * read, etc.)
		 */
		void printError(std::string operation);
		/// a wrapper for pcap_dispatch
		int pcapDispatch(u_char *userArg);
		/// sets the BPF filter for this interface
		int setFilter(bpf_u_int32 networkNum);
		char *getFilter() { return _filter; }
		bool getToCopy() { return _toCopy; }
		virtual void getNicStats() = 0;

	protected:
		/// pcap handle
		pcap_t *_handle;
		/// BPF filter expression
		char *_filter;
		/// compiled filter program
		struct bpf_program _filterProg;
		/// buffer for error message
		char _errBuf[PCAP_ERRBUF_SIZE];
		/// pointer to the pcap packet buffer pool singleton
		PcapPacketBufferPool *_bufPool;
};

/**
 * Defines a pcap savefile interface
 */
class PcapOfflineInterface : public PcapInterface {
	public:
		PcapOfflineInterface(const char *name, uint32_t batchSize, 
			char *filter = NULL, bool toCopy = true, 
			int snapLen = MAX_ETH_FRAME_SIZE_STAND);
		~PcapOfflineInterface();
		int open();
		InterfaceStatus read();
		void getNicStats() { }
	private:
		/// set of file descriptors being monitored
		struct pollfd _fds[1];
};

/**
 * Defines a pcap NIC interface
 */
class PcapOnlineInterface : public PcapInterface {
	public:
		PcapOnlineInterface(const char *name, 
			uint32_t batchSize = PCAP_DEFAULT_BATCH_SIZE, char *filter = NULL, 
			bool toCopy = true, int snapLen = MAX_ETH_FRAME_SIZE_STAND, 
			int timeOut = PCAP_DEFAULT_TIMEOUT_LEN);
		~PcapOnlineInterface();
		int open();
		InterfaceStatus read();
		void getNicStats(); 
		static void getNicInfo(const char *name, bpf_u_int32 *networkNum, 
			bpf_u_int32 *networkMask, char *errbuf);
		static char *getDefaultNic();
		static int getAllNics(std::list<char *>& nics);
		static void printAddress(bpf_u_int32 addr);
	
	private:
		/// read timeout value in millisecond
		int _timeOut;
		/// IPv4 address of the interface
		bpf_u_int32 _networkNum;
		/// IPv4 netmask of the interface
		bpf_u_int32 _networkMask;
		/// set of file descriptors being monitored
		struct pollfd _fds[2];
};

#endif //PCAP_INTERFACE_H

