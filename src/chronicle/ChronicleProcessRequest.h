// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLE_PROCESS_REQUEST_H
#define CHRONICLE_PROCESS_REQUEST_H

#include <atomic>
#include <inttypes.h>
#include <sys/time.h>
#include <pcap.h>
#include <string>
#include <linux/nfs.h>
#include <linux/nfs3.h>
#include "FifoList.h"	
#include "PacketBuffer.h"
#include "ChronicleConfig.h"
#include <cstring>
#include <list>

class Interface;
class PduDescriptor;

/**
 * A packet buffer descriptor that gets passed between different stages of the
 * Chronicle pipeline. The descriptor should contain all the information needed
 * to process a packet at every pipeline stage.
 */
class PacketDescriptor : public FifoNode<PacketDescriptor> {		
	public:		
		PacketDescriptor() 
		{
			refCount = 0; 
			#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
			allocated = false;
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
			pduDesc = NULL; 
			lastTouchingProcess = 0;
			#endif
		}
		/**
		 * returns the address where the corresponding ethernet frame is stored
		 * @returns the address of the corresponding ethernet frame
		 */
		unsigned char *getEthFrameAddress(); 
		void print();

		/// pcap header for the packet
		struct pcap_pkthdr pcapHeader;
		/// index of the descriptor/packet in the buffer pool (for tracking purposes)
		uint32_t packetBufferIndex;
		/// source IP address
		uint32_t srcIP;
		/// destination IP address
		uint32_t destIP;
		/// TCP sequence number (if applicable)
		uint32_t tcpSeqNum;
		/// TCP ack number (if applicable)
		uint32_t tcpAckNum;
		/// ref count (i.e., the number of PDUs per packet)
		std::atomic<unsigned int> refCount;
		/// address of the packet
		unsigned char *packetBuffer;
		/// next packet in the byte stream
		PacketDescriptor *next;
		/// previous packet in the byte stream
		PacketDescriptor *prev;
		/// pointer to the packet interface
		Interface *interface;
		/// source port 
		uint16_t srcPort;
		/// destination port
		uint16_t destPort;
		/// packet payload offset (where TCP/UDP payload starts)
		uint16_t payloadOffset;
		/// packet payload length
		uint16_t payloadLength;
		/// parsed offset
		uint16_t toParseOffset;
		/// visit count (i.e., RPC parse attempts)
		uint16_t visitCount;
		/// NIC ring id
		uint16_t ringId;
		/// transport protocol (e.g., UDP, TCP, etc.)
		uint8_t protocol;
		/// flag (in order, truncated, etc.)
		uint8_t flag;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL | CHRONICLE_DEBUG_STAT)
		/// whether packet is allocated
		std::atomic<bool> allocated;
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		/// pointer to the PDU descriptor that this pktDesc points to
		PduDescriptor *pduDesc;
		/// the last process that touched this descriptor
		uint8_t lastTouchingProcess;
		#endif
};

class PduDescriptor {
	public:
		/// PDU types
		typedef enum type {
			PDU_COMPLETE = 0, // complete PDU
			PDU_COMPLETE_HEADER = 1, // incomplete PDU with complete RPC header
			PDU_BAD = 2	// incomplete PDU (may not even be a PDU)
		} RpcPduType;

		/// constructor for "useful" PDUs
		PduDescriptor(PacketDescriptor *firstDesc, 
			PacketDescriptor *firstProgPktDesc,
			uint32_t pduLen,
			uint32_t xid, uint32_t prog, uint32_t progVersion, 
			uint32_t progProc, uint16_t rpcHeaderOffset, 
			uint16_t rpcProgOffset, uint16_t acceptState, 
			uint8_t msgType) 
			: firstPktDesc(firstDesc), firstPktDescRpcProg(firstProgPktDesc),
			  rpcPduLen(pduLen), rpcXid(xid), rpcProgram(prog), 
			  rpcProgramVersion(progVersion), 
			  rpcProgramProcedure(progProc), 
			  rpcHeaderOffset(rpcHeaderOffset),
			  rpcProgramStartOffset(rpcProgOffset), 
			  rpcAcceptState(acceptState), rpcMsgType(msgType) 
			  { next = NULL; refCount = 1; }
		/// constructors for "bad" PDUs
		PduDescriptor(PacketDescriptor *firstDesc, 
			PacketDescriptor *lastDesc) 
			: firstPktDesc(firstDesc), lastPktDesc(lastDesc)
			  { rpcPduType = PDU_BAD; }
		PduDescriptor(PacketDescriptor *firstDesc) 
			: firstPktDesc(firstDesc) { rpcPduType = PDU_BAD; }
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		/// tags packets with the last process
		void tagPduPkts(uint8_t processId);
		#endif

		virtual ~PduDescriptor() { }
		/// prints a PDU
		virtual void print() = 0;
		/// returns the number of packets in the PDU
		int getNumPackets();

		/// The first packet in a PDU
		PacketDescriptor *firstPktDesc;
		/// The last packet in a PDU
		PacketDescriptor *lastPktDesc;
		/// The first packet for the RPC program
		PacketDescriptor *firstPktDescRpcProg;
		/// The next PDU
		PduDescriptor *next;
		/// The length of this PDU
		uint32_t rpcPduLen;
		/// The transaction id for this PDU
		uint32_t rpcXid;
		/// The RPC program id for this PDU
		uint32_t rpcProgram;
		/// The RPC program version for this PDU
		uint32_t rpcProgramVersion;
		/// The RPC program procedure for this PDU
		uint32_t rpcProgramProcedure;
		/// ref count for the PDU
		std::atomic<unsigned int> refCount;
		/// The type of PDU (i.e., COMPLETE, COMPLETE_HEADER, and BAD)
		RpcPduType rpcPduType;
		/// The offset within packet where RPC header starts
		uint16_t rpcHeaderOffset;
		/// The offset within packet where the RPC program (e.g., NFS) starts
		uint16_t rpcProgramStartOffset;
		/// The offset within packet where the RPC program (e.g., NFS) ends
		uint16_t rpcProgramEndOffset;
		/// The accept state for reply PDUs
		uint16_t rpcAcceptState;
		/// The type of PDU (i.e., call or reply)
		uint8_t rpcMsgType;
		/// Record id assigned by DsWriter
		uint64_t dsRecordId;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		uint8_t lastTouchingProcess;
		#endif
};

class NfsV3PduDescriptor : public PduDescriptor {
	public:
		class Link;
		class DeviceSpec;
		class Fsstat;
		class Fsinfo;
		class Pathconf;
		class ChecksumRecord;

		/// Constructor for an NFS PDU
		NfsV3PduDescriptor(PacketDescriptor *firstPktDesc,
			PacketDescriptor *firstProgPktDesc, uint32_t pduLen,
			uint32_t xid, uint32_t progVersion, uint32_t progProc, 
			uint16_t rpcHeaderOffset, uint16_t rpcProgOffset, 
			uint16_t acceptState, uint8_t msgType) 
			: PduDescriptor(firstPktDesc, firstProgPktDesc, pduLen, xid, 
			  NFS_PROGRAM, progVersion, progProc, rpcHeaderOffset,
			  rpcProgOffset, acceptState, msgType)
			  { parsable = false; fhLen = 0; pktDesc[0] = NULL; }
		~NfsV3PduDescriptor();
		bool getFileName(unsigned char *name, uint16_t index, 
			PacketDescriptor *pktDesc);
		bool getPathName(unsigned char *name, uint16_t index,
			PacketDescriptor *pktDesc);
		bool getLink(Link *link);
		bool getDeviceSpec(DeviceSpec *spec);
		bool getFsstat(Fsstat *fsstat);
		bool getFsinfo(Fsinfo *fsinfo);
		bool getReaddirEntry(unsigned char *fileName, bool &eof);
		bool getReaddirplusEntry(unsigned char *fileName, bool &eof);
		bool getPathconf(Pathconf *pathconf);
		bool getFileHandle(uint16_t index, PacketDescriptor *pktDesc);
		std::string getFileHandleStr();
		void print();
		void printNULL();
		void printGETATTR();
		void printSETATTR();
		void printLOOKUP();
		void printACCESS();
		void printREADLINK();
		void printREAD();
		void printWRITE();
		void printCREATE();
		void printMKDIR();
		void printSYMLINK();
		void printMKNOD();
		void printREMOVE();
		void printRMDIR();
		void printRENAME();
		void printLINK();
		void printREADDIR();
		void printREADDIRPLUS();
		void printFSSTAT();
		void printFSINFO();
		void printPATHCONF();
		void printCOMMIT();
		void printFileHandle();
		void printFileAttr();
		void printSetAttr();

		/// file handle
		unsigned char fileHandle[NFS3_FHSIZE];
		/// file size in bytes
		uint64_t fileSizeBytes;
		/// used bytes in file
		uint64_t fileUsedBytes;
		/// file system id
		uint64_t fileSystemId;
		/// file inode
		uint64_t fileId;
		/// file modification time
		uint64_t fileModTime;
		/// offset for read/write
		uint64_t fileOffset;
		/// file handle length
		uint32_t fhLen;
		/// operation status
		uint32_t nfsStatus;
		/// file user id
		uint32_t fileUid;
		/// file group id
		uint32_t fileGid;
		/// byte count for READ, WRITE, and READDIRPLUS
		uint32_t byteCount;
		/// max byte count for READDIR and READDIRPLUS
		uint32_t maxByteCount;
		/// WRITE stable
		uint32_t writeStable;
		/// file/device type
		uint32_t type;
		/// file mode
		uint32_t fileMode;
		/// access mode
		uint32_t accessMode;
		/**
		 * misc index 0:
		 * - file name for LOOKUP, CREATE, MKDIR, REMOVE, RMDIR, and RENAME
		 * - data index for READ and WRITE
		 * - procedure-specific fields for 
		 *     FSSTAT, FSINFO, LINK, PATHCONF, READDIR, READDIRPLUS, READLINK
		 *     SYMLINK
		 */
		uint16_t miscIndex0;
		/**
		 * misc index 1:
		 * - file name for RENAME and SYMLINK
		 * - device name for MKNOD
		 * - TODO: dir attributes for LOOKUP, CREATE, MKDIR
		 */
		uint16_t miscIndex1;
		/**
		 * misc index 2:
		 * - the "to" file handle for RENAME
		 */
		uint16_t miscIndex2;
		/// parsable PDU (i.e., parsed all fields of the NFS PDU)
		bool parsable;
		/// packets corresponding to miscIndex[0-2]
		PacketDescriptor *pktDesc[3];
		/// List of the data hashes for read/write replies/calls
		std::list<ChecksumRecord> dataChecksums;
};

/**
 * A class that constructs a PDU out of all unparsable packets
 */
class BadPduDescriptor : public PduDescriptor {
	public:
		BadPduDescriptor(PacketDescriptor *firstPktDesc, 
			PacketDescriptor *lastPktDesc); 
		~BadPduDescriptor() { }
		void print();
};

class NfsV3PduDescriptor::DeviceSpec {
public:
    uint32_t majorNum;
    uint32_t minorNum;
};

class NfsV3PduDescriptor::Link {
	public:
		unsigned char dirFileHandle[NFS3_FHSIZE];
		unsigned char linkName[NFS_MAXNAMLEN + 1];
};

class NfsV3PduDescriptor::Fsstat {
	public:
		void print()
		{
			printf("tbytes:%lu fbytes:%lu afbytes:%lu\n"
					"tfslots:%lu ffslots:%lu affslots:%lu invarsec:%u\n",
					totalBytes, freeBytes, availFreeBytes, totalFileSlots,
					freeFileSlots, availFreeFileSlots, invarSeconds);
		}

		uint64_t totalBytes;
		uint64_t freeBytes;
		uint64_t availFreeBytes;
		uint64_t totalFileSlots;
		uint64_t freeFileSlots;
		uint64_t availFreeFileSlots;
		uint32_t invarSeconds;
};

class NfsV3PduDescriptor::Fsinfo {
	public:
		void print()
		{ 
			printf("rtmax:%u rtpref:%u rtmult:%u wtmax:%u wtpref:%u "
					"wtmult:%u\ndtpref:%u maxFileSize:%lu delta:%lu "
					"prop:%#x\n", rtmax, rtpref, rtmult, wtmax, wtpref, 
					wtmult, dtpref, maxFileSize, timeDelta, properties);
		}

		uint32_t rtmax;
		uint32_t rtpref;
		uint32_t rtmult;
		uint32_t wtmax;
		uint32_t wtpref;
		uint32_t wtmult;
		uint32_t dtpref;
		uint64_t maxFileSize;
		uint64_t timeDelta;
		uint32_t properties;
};

class NfsV3PduDescriptor::Pathconf {
	public:
		void print()
		{
			printf("linkmax:%u namemax:%u notrunc:%u chownrestrict:%u "
					"caseinsens:%u casepreserv:%u\n",
					linkMax, nameMax, noTrunc, chownRestrict, 
					caseInsensitive, casePreserving);
		}

		uint32_t linkMax;
		uint32_t nameMax;
		uint32_t noTrunc;
		uint32_t chownRestrict;
		uint32_t caseInsensitive;
		uint32_t casePreserving;
};

class NfsV3PduDescriptor::ChecksumRecord {
public:
    ChecksumRecord(uint64_t xsum, uint64_t off) :
        offset(off), checksum(xsum) { }

    uint64_t offset;
    uint64_t checksum;
};

#endif // CHRONICLE_PROCESS_REQUEST_H

