// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef TOPOLOGYMAP_H
#define TOPOLOGYMAP_H

#include <vector>
#include <bitset>
#include "LogicalProcessor.h"

class LogicalProcessor;

class TopologyMap {
	public:
		TopologyMap();
		~TopologyMap();
		/**
		 * Initializes a TopologyMap object based on the CPU/cache topology
		 * @returns the number of virtual processors (the number of threads if HT is
		 * available or the number of cores otherwise) or -1 (upon error)
		 */
		int init();
		/// Prints CPU/cache topology
		void print();
		/// Verfies if it is an Intel processor
		bool isIntel();
		/** 
		 * Populates processor-specific information in the object
		 * @returns 0 upon success or -1 upon error
		 */
		int getProcessorInfo();
		/**
		 * Sets up a list of logical processor numbers that are on the same core
		 * @param[in] processorNum The input logical processor number 
		 * @param[inout] list The list of logical processor numbers that share the same core as processorNum
		 * @returns 0 for success -1 for failure
		 */
		int sameCore(int processorNum, std::list<int>& list);
		/**
		 * Sets up a list of logical processor numbers that are on the same socket
		 * @param[in] processorNum The input logical processor number 
		 * @param[inout] list The list of logical processor numbers that share the same socket as processorNum
		 * @returns 0 for success -1 for failure
		 */
		int sameSocket(int processorNum, std::list<int>& list);
		/**
		 * Sets up a list of logical processor numbers that share a certain cache level
		 * @param[in] processorNum The input logical processor number
		 * @param[in] level The cache level we are interested in 
		 * @param[inout] list The list of logical processor numbers that share the same cache level s processorNum
		 * @returns 0 for success -1 for failure
		 */
		int sameCache(int processorNum, uint8_t level, std::list<int>& list);
		/**
		 * Sets up a list of logical processor numbers that are on the same socket but not on the same core
		 * @param processorNum The input logical processor number
		 * @param[in] level The cache level we are interested in 
		 * @param[inout] list The list of logical processor numbers that are on the same socket but not on the same core
		 * @returns 0 for success -1 for failure
		 */
		int getPriorityList(int processorNum, std::list<int>& list);
		/// CPU features specification
		enum Features {HT=0, x2APIC, DCA};

    /**
     * Get the LogicalProcessor object associated with a processor number.
     * @param[in] processorNum The logical processor number (0..n-1)
     * @returns a pointer to the LP object
     */
    LogicalProcessor *getProcessor(unsigned processorNum);
    /**
     * Get the number of logical processors in the system.
     * @returns the number of processors in the system
     */
    unsigned numProcessors();

	private:
		/// Registers used by CPUID
		uint32_t eax, ebx, ecx, edx;
		/// Max opcode (eax) supported by CPUID
		uint32_t cpuidLeaf;
		/// CPU manufacturer
		char manufacturer[13];
		/// CPU family
		uint8_t family;
		/// CPU model
		uint8_t model;
		/// CPU stepping
		uint8_t stepping;
		/// CPU brand ID
		uint8_t brandID;
		/// CPU features
		std::bitset<3> features;
		/// Max number of logical processors (typically hyperthreads)
		uint8_t maxLogicalProcessors;
		/// Max number of logical processors per core
		uint8_t maxLogicalProcessorsPerCore;
		/// Max number of logical processors per package
		uint8_t maxLogicalProcessorsPerPackage;
		/// Max number of cores
		uint8_t maxNumCores;
		/// A vector of all the logical processors in the system
		std::vector<LogicalProcessor *> processors;	
};

/**
 * Returns all the bits from start to end (including start and end) in num
 * @param[in] start the starting bit
 * @param[in] end the ending bit
 * @returns the bit range from start to end
 */
unsigned int getBitRange(unsigned int num, unsigned int start, unsigned int end);

/**
 * Returns the number of bits (i.e., log2) of a given number
 * @param num the input number
 * @returns the number of bits to represent num
 */
unsigned int getFieldBitWidth(unsigned int num);

/**
 * Sets the affinity of the running thread to LogicalProcessor num
 * @param num the logical CPU number
 * @returns 0 for success or -1 for error
 */
int setAffinity(int logicalProcessorNum);
#endif
