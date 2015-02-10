// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

extern "C" {
	#include <sched.h>
}

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "TopologyMap.h"

#define CPUID1(eax)						\
	asm volatile ("cpuid"				\
		: "=a" (eax), "=b" (ebx),		\
		  "=c" (ecx), "=d" (edx) 		\
		: "a" (eax)						\
		)
#define CPUID2(eax, ecx)				\
	asm volatile ("cpuid"				\
		: "=a" (eax), "=b" (ebx),		\
		  "=c" (ecx), "=d" (edx) 		\
		: "a" (eax), "c" (ecx)			\
		)

TopologyMap::TopologyMap()
{}

TopologyMap::~TopologyMap()
{
	std::vector<LogicalProcessor *>::iterator it;

	for (it = processors.begin(); it != processors.end(); it++) 
		delete *it;
}

int
TopologyMap::init()
{
	if(getProcessorInfo() == -1) {
		fprintf(stderr, "TopologyMap::init: incomplete processor information\n");
		return -1;
	}

	for (int lpNum=0; lpNum < maxLogicalProcessors; lpNum++) {
		int cpuLevel=0;
		uint8_t threadID=0, coreID=0, socketID=0;
		uint32_t apicID=0;

		if (setAffinity(lpNum) == -1)
			return -1;

		if (cpuidLeaf >= 0xb) {	//using x2APIC feature (if supported) to get processor topology
			eax = 0xb;
			for (cpuLevel=0; (eax || ebx); cpuLevel++) {
				eax = 0xb; ecx = cpuLevel;
				CPUID2(eax, ecx);

				switch(cpuLevel) {
					case 0:
						if (getBitRange(ebx, 0, 15) != maxLogicalProcessorsPerCore)
							fprintf(stderr, "TopologyMap::init: descrepency in the number of logical processors per core (%u vs %u)!\n", getBitRange(ebx, 0, 15), maxLogicalProcessorsPerCore);
						threadID = edx;
						coreID = edx >> getBitRange(eax, 0, 4);
						break;
					case 1:
						socketID = edx >> getBitRange(eax, 0, 4);
						break;
					case 2:
						break;
				}
			}
		}
		else if (family == 0x6) {	//getting processor topology the old-school way for P6 processors
			unsigned int processedBits=0, bitWidth;
			const unsigned int apicIDLen = 8;

			eax = 1;
			CPUID1(eax);

			apicID = getBitRange(ebx, 24, 31);

			bitWidth = getFieldBitWidth(maxLogicalProcessorsPerCore - 1);
			if (!bitWidth)
				threadID = 0;
			else {
				threadID=getBitRange(apicID, 0, bitWidth - 1);
				processedBits += bitWidth;
			}

			bitWidth = getFieldBitWidth(maxNumCores - 1);
			if (!bitWidth)
				coreID = 0;
			else {
				coreID = getBitRange(apicID, processedBits, bitWidth - 1);
				processedBits += bitWidth;
			}

			bitWidth = apicIDLen - processedBits;
			if (!bitWidth)
				socketID = 0;
			else {
				socketID = getBitRange(apicID, processedBits, bitWidth - 1);
				processedBits += bitWidth;
			}

			if (processedBits != apicIDLen)
				fprintf(stderr, "Error!\n");

		} else {
			fprintf(stderr, "TopologyMap::init: no support for non-P6 (0x6) family processors!\n");
			return -1;
		}			
		
		LogicalProcessor *lp = new LogicalProcessor(cpuLevel-1, lpNum, threadID, coreID, socketID);
		processors.push_back(lp);

		//getting cache topology (section 5.1.5.1.1 of [1])
		eax = 1;
		CPUID1(eax);

		apicID = getBitRange(ebx, 24, 31);

		if (maxLogicalProcessorsPerCore == 1 && apicID != coreID) 
			fprintf(stderr, "TopologyMap::init: *WARNING* core ID (0x%x) and APIC ID (0x%x) don't match!\n", coreID, apicID);
		else if (maxLogicalProcessorsPerCore > 1 && apicID != threadID) 
			fprintf(stderr, "TopologyMap::init: *WARNING* thread ID (0x%x) and APIC ID (0x%x) don't match!\n", threadID, apicID);

		eax = 4;
		for (int level=0; getBitRange(eax, 0, 4); level++) {	//TODO: get rid of getBitRange?
			uint32_t cacheSize, tmp, cacheMaskWidth;
			uint16_t cacheLineSize;
			uint8_t cacheLevel, inclusive;
			CacheType type;

			eax = 4; ecx = level;
			CPUID2(eax, ecx);

			if(!getBitRange(eax, 0, 4)) 
				break;
				
			cacheSize = ((getBitRange(ebx, 22, 31) + 1) * 
				(getBitRange(ebx, 12, 21) + 1) * 
				(getBitRange(ebx, 0, 11) + 1) * (ecx + 1));
			cacheLineSize = getBitRange(ebx, 0, 11) + 1;
			cacheLevel = getBitRange(eax, 5, 7);
			inclusive = getBitRange(edx, 1, 1);

			switch(getBitRange(eax, 0, 4))
			{
				case 0:
					type = NULLCACHE;
					break;
				case 1:
					type = DATA;
					break;
				case 2:
					type = INST;
					break;
				case 3:
					type = UNIFIED;
					break;
				default:
					type = OTHER;
			}

			tmp = getBitRange(eax, 14, 25);
			for (cacheMaskWidth=0; tmp; cacheMaskWidth++) 
				tmp >>= 1;

			lp->insertCache(apicID & (-1 << cacheMaskWidth), cacheLevel, cacheSize, cacheLineSize, type, inclusive);
		}
	}
	return maxLogicalProcessors;
}

void TopologyMap::print()
{
	std::vector<LogicalProcessor *>::const_iterator it;

	printf("=====================================================================\n");
	printf("%s processor:\tfamily=0x%x model=0x%x stepping=0x%x leaf=0x%x\n", manufacturer, family, model, stepping, cpuidLeaf);
	printf("\t\t\t%u logical processors (%u per core), %u cores\n", maxLogicalProcessors, maxLogicalProcessorsPerCore, maxNumCores);
	printf("\t\t\tfeatures: HT=%u, x2APIC=%u, DCA=%u\n", features.test(HT), features.test(x2APIC), features.test(DCA));
	printf("=====================================================================\n");

	for (it = processors.begin(); it != processors.end(); it++)
		(*it)->print();
}

bool TopologyMap::isIntel()
{
	eax = 0;
	CPUID1(eax); 

	memcpy(manufacturer, &ebx, sizeof(ebx));
	memcpy(manufacturer + sizeof(ebx), &edx, sizeof(edx));
	memcpy(manufacturer + sizeof(ebx) + sizeof(edx), &ecx, sizeof(ecx));
	manufacturer[12] = '\0';
	cpuidLeaf = eax;

	if (strcmp(manufacturer, "GenuineIntel"))
		return false;

	return true;
}

int TopologyMap::getProcessorInfo()
{
	if(!isIntel()) {
		fprintf(stderr, "TopologyMap::getProcessorInfo: No support for non-Intel processors\n");
		return -1;
	}

	eax = 1;
	CPUID1(eax);

	family = getBitRange(eax, 20, 27) + getBitRange(eax, 8, 11);
	model = (getBitRange(eax, 16, 19) << 4) + getBitRange(eax, 4, 7);
	stepping = getBitRange(eax, 0, 3);
	brandID = getBitRange(ebx, 0, 7);
	/* The number of APIC IDs reserverd for this package.
	 * Physical package can refer to a socket or to a core [2].
	 * Therefore, maxLogicalProcessors is the number of logical processors for this package. */
	maxLogicalProcessorsPerPackage = getBitRange(ebx, 16, 23);
	maxLogicalProcessors = sysconf(_SC_NPROCESSORS_CONF);

	/* TODO: DEL
	long nprocessors;
	if ((nprocessors = sysconf(_SC_NPROCESSORS_CONF)) != maxLogicalProcessors) {
		fprintf(stderr, "TopologyMap::getProcessorInfo: *WARNING* sysconf(_SC_NPROCESSORS_CONF) and maxLogicalProcessors don't match (%ld vs %u)!\n", nprocessors, maxLogicalProcessors);
		if (nprocessors == -1) {
			perror("sysconf(_SC_NPROCESSORS_CONF)");
			return -1;
		}
		// maxLogicalProcessors = nprocessors;		
	}
	*/

	if (edx & 1<<28)
		features.set(HT);

	if (ecx & 1<<21)
		features.set(x2APIC);

	if (ecx & 1<<18)
		features.set(DCA);			//supports the ability to prefetch data from a memory-mapped device

	if (cpuidLeaf >= 4) {
		eax = 4; ecx = 0;			//section 5.1.5 of [1]
		CPUID2(eax, ecx);			

		maxNumCores = getBitRange(eax, 26, 31) + 1;
		maxLogicalProcessorsPerCore = maxLogicalProcessorsPerPackage / maxNumCores;
	} else
		return -1;

	return 0;	
}

int TopologyMap::sameCore(int logicalProcessorNum, std::list<int>& l)
{
	LogicalProcessor *lp;
	std::vector<LogicalProcessor *>::const_iterator it;
	uint8_t coreID;

	if (logicalProcessorNum < 0 || logicalProcessorNum >= maxLogicalProcessors) {
		fprintf(stderr, "TopologyMap::sameCore: out of range input!\n");
		return -1;
	}

	lp = processors.at(logicalProcessorNum);
	coreID = lp->getCoreID();

	for (it = processors.begin(); it != processors.end(); it++)
		if ((*it)->getCoreID() == coreID && 
				(*it)->getLogicalProcessorID() != logicalProcessorNum) 
			l.push_back((*it)->getLogicalProcessorID());

	return 0;
}

int TopologyMap::sameSocket(int logicalProcessorNum, std::list<int>& l)
{
	LogicalProcessor *lp;
	std::vector<LogicalProcessor *>::const_iterator it;
	uint8_t socketID;

	if (logicalProcessorNum < 0 || logicalProcessorNum >= maxLogicalProcessors) {
		fprintf(stderr, "TopologyMap::sameCore: out of range input!\n");
		return -1;
	}

	lp = processors.at(logicalProcessorNum);
	socketID = lp->getSocketID();

	for (it = processors.begin(); it != processors.end(); it++)
		if ((*it)->getSocketID() == socketID && 
				(*it)->getLogicalProcessorID() != logicalProcessorNum)
			l.push_back((*it)->getLogicalProcessorID());

	return 0;
}

int TopologyMap::sameCache(int logicalProcessorNum, uint8_t level, std::list<int>& l)
{
	//LogicalProcessor *lp;
	std::vector<LogicalProcessor *>::const_iterator it;

	if (logicalProcessorNum < 0 || logicalProcessorNum >= maxLogicalProcessors) {
		fprintf(stderr, "TopologyMap::sameCore: out of range input!\n");
		return -1;
	}

	//lp = processors.at(logicalProcessorNum); //TODO: incomplete
	return 0;
}
	
int TopologyMap::getPriorityList(int logicalProcessorNum, std::list<int>& list)
{
	std::list<int> l;
	std::list<int>::iterator it1, it2;
	unsigned int remained; 
	
	sameCore(logicalProcessorNum, l);
	sameSocket(logicalProcessorNum, list);
	remained = l.size();

	for (it1 = list.begin(); it1 != list.end() && remained; ) {
		for (it2 = l.begin(); it2 != l.end(); it2++) {
			if (*it1 == *it2) {
				remained--;
				it1 = list.erase(it1);
			} else
				it1++;
		}
	}
	
	return 0;
}

LogicalProcessor *
TopologyMap::getProcessor(unsigned processorNum)
{
    if (processorNum < processors.size()) {
        return processors[processorNum];
    } else {
        return 0;
    }
}

unsigned
TopologyMap::numProcessors()
{
    return processors.size();
}

unsigned int 
getBitRange(unsigned int num, unsigned int start, unsigned int end)
{
	unsigned int shiftLeft, shiftRight;

	if (start > 31 || end < start || end > 31)
		fprintf(stderr, "getBitRange: invalid range\n");
	shiftLeft = sizeof(unsigned int)*8 - 1 - end;
	shiftRight = start + shiftLeft;

	return (num << shiftLeft) >> shiftRight; 
}

unsigned int 
getFieldBitWidth(unsigned int num)
{
	unsigned int i;

	if (!num) return 0;

	for (i=0; num; num >>= 1, i++) ;	
	return i;
}

int 
setAffinity(int logicalProcessorNum)
{
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(logicalProcessorNum, &mask); /* //had to do the ugly hack below to get past g++ old-style cast warnings for the sched.h macros!
	(__extension__					    
	 ({ size_t __cpu = (logicalProcessorNum);						      
	  __cpu < 8 * (sizeof (cpu_set_t))						      
	  ? ((static_cast<__cpu_mask *> ((&mask)->__bits))[__CPUELT (__cpu)]		      
		  |= static_cast<__cpu_mask> (1 << ((__cpu) % __NCPUBITS)))
	  : 0; }));*/

	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask)) {
		perror("sched_setaffinity");
		return 1;
	}
	return 0;
}

