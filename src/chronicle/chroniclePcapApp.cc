// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <list>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "Scheduler.h"
#include "Chronicle.h"
#include "ChronicleConfig.h"
#include "PcapInterface.h"

class SupervisorCb : public Chronicle::CompletionCb {
	public:
		void operator()(Chronicle *c, Chronicle::InterfaceStatCb *icb) {
			if (c) {
				std::list<InterfaceStat *>::iterator it;
				for (it = c->doneInterfaceStats.begin(); 
						it != c->doneInterfaceStats.end(); it++) {
					(*icb)(*it);
				}
			}
		}
};

class StatCb : public Chronicle::InterfaceStatCb {
	public:
		void operator()(InterfaceStat *stat) {
			if (stat) {
				char buf1[30], buf2[30];
				time_t statTime = stat->getTime().tv_sec;
				struct tm *statTm = localtime(&statTime);
				strftime(buf1, sizeof(buf1), "%D %T", statTm);
				snprintf(buf2, sizeof(buf2), "%s:%06u", buf1, 
					static_cast<unsigned> (stat->getTime().tv_usec));
				std::cout << "================================================"
					<< "=================\n"
					<< buf2 << std::endl
					<< "Interface: " << stat->getName() << std::endl
					<< "Type: " << stat->getType() << ", "
					<< "Status: ";
				switch(stat->getStatus()) {
					case Interface::IF_ACTIVE:
						std::cout << "ACTIVE\n";
						break;
					case Interface::IF_ERR:
						std::cout << "EXIT ERROR\n";
						break;
					case Interface::IF_DONE:
						std::cout << "EXIT NORMAL\n";
						break;
					case Interface::IF_IDLE:
						std::cout << "IDLE\n";
						break;
					case Interface::IF_FULL:
						std::cout << "BUFFER FULL\n";
						break;
					default:
						std::cout << std::endl;
				}
				std::cout << "Chronicle Packets read: " 
					<< stat->getPacketsRead() << ", "
					<< "Chronicle Packets dropped: " 
					<< stat->getPacketsDropped() << std::endl
					<< "NIC packets read: " << stat->getNicPacketsRead() 
					<< ", "
					<< "NIC packets dropped: " << stat->getNicPacketsDropped() 
					<< std::endl
					<< "Batch size: " << stat->getBatchSize() 
					<< ", "
					<< "Max batch size read: " << stat->getMaxBatchSizeRead()
					<< ", "
					<< "Complete batches: " << stat->getFilledBatches()
					<< std::endl;
			}
		}
};

void usage()
{
	//TODO: man page
	std::cerr << "./chronicle_pcap [-s pcap_savefile] " 
		"[-i pcap_nic|ALL|DEFAULT]\n"
		"\t[-n[nfs_pipeline_num] | -p[pcap_w/o_parse_pipeline_num]]\n"
		"\t[-D[dataseries_output_module_num] | -P[pcap_output_module_num]]\n"
		"\t[-a (to_enable_analytics)]\n"
		"\t[-o output_dir] [-b batch_size] [-l snapshot_len] [-X]\n"
		"\t[-f \"filter_expression\"]\n"
		"\t[-t num_libtask_threads] [-B (to_bind_libtask_threads)]\n";
}

// rounds down to the nearest power of two
unsigned roundDownPowerOf2(int num)
{
	unsigned bits = 0;
	do {
		num >>= 1;
		bits++;
	} while (num);
	return 1 << (bits - 1);
}

int main(int argc, char **argv)
{
	std::list<char *> pcapFiles, pcapNics;
	std::list<char *>::const_iterator it;
	uint32_t batchSize = PCAP_DEFAULT_BATCH_SIZE;
    uint32_t snapLen = MAX_ETH_FRAME_SIZE_JUMBO;
	uint32_t pipelineNum = DEFAULT_PIPELINE_NUM;
    uint32_t numInterfaces;
    uint8_t pipelineType = NFS_PIPELINE;
	uint8_t outputFormat = NO_OUTPUT;
	uint32_t numOutputModules = DEFAULT_OUTPUT_MODULE_NUM;
	char filter[PCAP_MAX_BPF_FILTER_LEN] = "";
	bool toCopy = true, bindCpu = false, enableAnalytics = false;;
	char option;
	unsigned threads = Scheduler::numProcessors();
	SupervisorCb supervisorCb;
	StatCb statCb;

	if (argc == 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	while ((option = getopt(argc, argv, 
			"aBb:D::f:hi:l:n::P::p::o:s:t:X")) != -1) {
		switch (option) {
			case 'a':	/* inline analytics */
				enableAnalytics = true;
				break;	
			case 'B':	/* enable CPU binding for libtask */
				bindCpu = true;
				break;
			case 'b':	/* batch size to read packets */
				batchSize = atoi(optarg);
				break;
			case 'D':	/* DataSeries output */
				outputFormat = DS_OUTPUT;
				if (optarg) {
					if (atoi(optarg) > 0 
							&& atoi(optarg) <= MAX_OUTPUT_MODULE_NUM)
						numOutputModules = roundDownPowerOf2(atoi(optarg));
				}
				break;
			case 'h':
				usage();
				exit(0);
			case 'f':	/* BPF filter */
				strcpy(filter, optarg);
				break;
			case 'i':	/* add a network interface */
				pcapNics.push_back(optarg);
				break;
			case 'l':	/* snapshot length */
				snapLen = atoi(optarg);
				break;
			case 'n':	/* NFS pipeline */
				pipelineType = NFS_PIPELINE;
				if (optarg) {
					if (atoi(optarg) > 0 
							&& atoi(optarg) <= MAX_PIPELINE_NUM)
						pipelineNum = roundDownPowerOf2(atoi(optarg));
				}
				break;
			case 'P':	/* Pcap output */
				outputFormat = PCAP_OUTPUT;
				if (optarg) {
					if (atoi(optarg) > 0 
							&& atoi(optarg) <= MAX_OUTPUT_MODULE_NUM)
						numOutputModules = roundDownPowerOf2(atoi(optarg));
				}
				break;
			case 'p':	/* pcap pipeline (no parsing) */
				pipelineType = PCAP_PIPELINE;
				if (optarg) {
					if (atoi(optarg) > 0 
							&& atoi(optarg) <= MAX_PIPELINE_NUM)
						pipelineNum = roundDownPowerOf2(atoi(optarg));
				}
				break;			
			case 'o':	/* trace output directory */
				traceDirectory = optarg;
				break;
			case 's':	/* add a pcap "savefile" */
				pcapFiles.push_back(optarg);
				break;
			case 't':	/* number of libtask threads */
				threads = atoi(optarg);
				break;
            case 'X':   /* disable checksum & IP extents for DS pipeline */
                dsFileSize = DS_DEFAULT_FILE_SIZE_SMALL;
                dsExtentSize = DS_DEFAULT_EXTENT_SIZE_SMALL;
                dsEnableIpChecksum = false;
                break;
			case '?':
				usage();			
				exit(EXIT_FAILURE);
		}
	}

	// error handling
	numInterfaces = pcapFiles.size() + pcapNics.size();
	if (!numInterfaces) {
		std::cerr 
			<< "ERROR: At least one network interface or pcap savefile needs"
			" to be specified!\n";
		usage();
		exit(EXIT_FAILURE);
	}
	if (enableAnalytics && pipelineType != NFS_PIPELINE) {
		std::cout << "ERROR: Analytics requires NFS pipeline to be set!\n";
		usage();
		exit(EXIT_FAILURE);
	}
	if (pipelineType == NFS_PIPELINE && outputFormat == NO_OUTPUT 
			&& !enableAnalytics) {
		std::cout << "=====================================\n"
			<< "WARNING: No traces will be collected!\n"
			<< "=====================================\n";
	}

	// starting Chronicle
	Scheduler::initScheduler();
	Chronicle *chronicle = new Chronicle(numInterfaces, fileno(stdin), 
		pipelineType, pipelineNum, outputFormat, numOutputModules, snapLen, 
		enableAnalytics, &supervisorCb, &statCb);

	// adding pcap savefiles
	for (it = pcapFiles.begin(); it != pcapFiles.end(); it++) {
		Interface *i = new PcapOfflineInterface(*it, batchSize, filter, toCopy, 
			snapLen);		
		chronicle->addInterface(i); 
	}

	// adding pcap nics
	for (it = pcapNics.begin(); it != pcapNics.end(); it++) {
		Interface *i = new PcapOnlineInterface(*it, batchSize, filter, toCopy, 
			snapLen);		
		chronicle->addInterface(i); 
	}

	Scheduler::startSchedulers(threads, chronicle, bindCpu);

	return 0;
}
