// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <arpa/inet.h>
#include <cstring>
#include <sys/time.h>
#include <time.h>
#include <sstream>
#include "ChronicleProcessRequest.h"
#include "RpcParser.h"
#include "TcpStreamNavigator.h"

extern uint32_t bitRange(uint32_t num, uint8_t start, uint8_t end);

unsigned char *
PacketDescriptor::getEthFrameAddress() 
{ 
	return packetBuffer;
}

void
PacketDescriptor::print()
{
	printf("--------------------------\n");
	char buf1[30], buf2[30];
	time_t statTime = pcapHeader.ts.tv_sec;
	struct tm *statTm = localtime(&statTime);
	strftime(buf1, sizeof(buf1), "%D %T", statTm);
	snprintf(buf2, sizeof(buf2), "%s:%06u", buf1, 
		static_cast<unsigned> (pcapHeader.ts.tv_usec));
	printf("%s\n", buf2);
	printf("index: %u visitCount: %u refCount:%u\n", packetBufferIndex, 
		visitCount, refCount.load());
	printf("pkt len: %u pkt caplen: %u\n", pcapHeader.len, pcapHeader.caplen);
	printf("srcIP: %u.%u.%u.%u destIP: %u.%u.%u.%u\n", 
		bitRange(srcIP, 24, 31), bitRange(srcIP, 16, 23), 
		bitRange(srcIP, 8, 15), bitRange(srcIP, 0, 7), 
		bitRange(destIP, 24, 31), bitRange(destIP, 16, 23), 
		bitRange(destIP, 8, 15), bitRange(destIP, 0, 7));
	printf("srcPort: %u destPort: %u\n", srcPort, destPort);
	printf("payload len: %u payload offset: %u\n", payloadLength, payloadOffset);
	printf("tcp seq: %u tcp ack: %u flag: %u\n", tcpSeqNum, tcpAckNum, flag);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
	printf("lastTouchingProcess: %u", lastTouchingProcess);
	printf(" pduDesc: %p\n", pduDesc);
	#endif
}

int
PduDescriptor::getNumPackets()
{
	int i = 1;
	PacketDescriptor *pktDesc = firstPktDesc;
	while (pktDesc != lastPktDesc) {
		i++;
		if (pktDesc == NULL)
			printf("i:%d\n", i);
		pktDesc = pktDesc->next;
	}
	return i;
}

#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
void
PduDescriptor::tagPduPkts(uint8_t processId)
{
	for (PacketDescriptor *pktDesc = firstPktDesc; pktDesc != lastPktDesc; 
			pktDesc = pktDesc->next) {
		pktDesc->lastTouchingProcess = processId;
		pktDesc->pduDesc = this;
	}
}
#endif

NfsV3PduDescriptor::~NfsV3PduDescriptor()
{
}

bool
NfsV3PduDescriptor::getFileName(unsigned char *name, uint16_t index,
	PacketDescriptor *pktDesc)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, index, pktDesc))
		goto err;
	if (!streamNavigator->getStringPdu(name, NFS_MAXNAMLEN))
		goto err;	
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool
NfsV3PduDescriptor::getPathName(unsigned char *name, uint16_t index,
	PacketDescriptor *pktDesc)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, index, pktDesc)) 
		goto err;
	if (!streamNavigator->getStringPdu(name, NFS_MAXPATHLEN))
		goto err;	
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool 
NfsV3PduDescriptor::getLink(Link *link)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;
	if (!streamNavigator->getUint32Pdu(&fhLen))
		goto err;
	if (fhLen > NFS3_FHSIZE)
		goto err;
	if (!streamNavigator->getBytesPdu(&link->dirFileHandle[0], fhLen))
		goto err;
	if (!streamNavigator->getStringPdu(&link->linkName[0], NFS_MAXPATHLEN))
		goto err;	
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool
NfsV3PduDescriptor::getDeviceSpec(DeviceSpec *spec)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex1, pktDesc[1])) 
		goto err;
	if (!streamNavigator->getUint32Pdu(&spec->majorNum))
		goto err;
	if (!streamNavigator->getUint32Pdu(&spec->minorNum))
		goto err;
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool 
NfsV3PduDescriptor::getFsstat(Fsstat *fsstat)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->totalBytes))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->freeBytes))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->availFreeBytes))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->totalFileSlots))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->freeFileSlots))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsstat->availFreeFileSlots))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsstat->invarSeconds))
		goto err;
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool 
NfsV3PduDescriptor::getFsinfo(Fsinfo *fsinfo)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->rtmax))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->rtpref))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->rtmult))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->wtmax))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->wtpref))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->wtmult))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->dtpref))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsinfo->maxFileSize))
		goto err;
	if (!streamNavigator->getUint64Pdu(&fsinfo->timeDelta))
		goto err;
	if (!streamNavigator->getUint32Pdu(&fsinfo->properties))
		goto err;
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool
NfsV3PduDescriptor::getReaddirEntry(unsigned char *fileName, bool &eof)
{
	uint32_t follows;	
	
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;	
	if (!streamNavigator->getUint32Pdu(&follows))
		goto err;
	if (follows == 1) {
		if (!streamNavigator->getUint64Pdu(&fileId)) 
			goto err;
		if (!streamNavigator->getStringPdu(fileName, NFS_MAXNAMLEN)) 
			goto err;
		if (!streamNavigator->skipBytesPdu(8)) // skipping cookie
			goto err;
		miscIndex0 = streamNavigator->getIndex();		
		pktDesc[0] = streamNavigator->getPacketDesc();
	} else 
		eof = true;
	delete streamNavigator;
	return true;
err:
	parsable = false;
	delete streamNavigator;
	return false;
}

bool
NfsV3PduDescriptor::getReaddirplusEntry(unsigned char *fileName, bool &eof)
{
	uint32_t follows;	
	
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;	

	if (!streamNavigator->getUint32Pdu(&follows))
		goto err;
	if (follows == 1) {
		if (!streamNavigator->getUint64Pdu(&fileId)) 
			goto err;
		if (!streamNavigator->getStringPdu(fileName, NFS_MAXNAMLEN)) 
			goto err;
		if (!streamNavigator->skipBytesPdu(8)) // skipping cookie
			goto err;
		
		// parsing post op attributes
		if (!streamNavigator->getUint32Pdu(&follows))
			goto err;
		if (follows == 1) {
			if (!streamNavigator->getUint32Pdu(&type))
				goto err;
			if (!streamNavigator->getUint32Pdu(&fileMode))
				goto err;
			else 
				fileMode &= 0xfff;
			if (!streamNavigator->skipBytesPdu(4)) // ignoring nlink
				goto err;
			if (!streamNavigator->getUint32Pdu(&fileUid))
				goto err;
			if (!streamNavigator->getUint32Pdu(&fileGid))
				goto err;
			if (!streamNavigator->getUint64Pdu(&fileSizeBytes))
				goto err;
			if (!streamNavigator->getUint64Pdu(&fileUsedBytes))
				goto err;
			if (!streamNavigator->skipBytesPdu(8)) // ignoring rdev
				goto err;
			if (!streamNavigator->getUint64Pdu(&fileSystemId))
				goto err;
			if (!streamNavigator->getUint64Pdu(&fileId))
				goto err;
			if (!streamNavigator->skipBytesPdu(8)) // ignoring access time
				goto err;
			if (!streamNavigator->getUint64Pdu(&fileModTime))
				goto err;
			if (!streamNavigator->skipBytesPdu(8)) // ignoring ctime
				goto err;
		}
		
		// parsing file handle
		if (!streamNavigator->getUint32Pdu(&follows))
			goto err;
		if (follows == 1) {
			if (streamNavigator->getUint32Pdu(&fhLen)) {
				if (fhLen > NFS3_FHSIZE)
					goto err;
				if (!streamNavigator->getBytesPdu(&fileHandle[0], fhLen))
					goto err;
			} else
				goto err;
		}
		miscIndex0 = streamNavigator->getIndex();		
		pktDesc[0] = streamNavigator->getPacketDesc();
	} else 
		eof = true;
	delete streamNavigator;
	return true;
err:
	parsable = false;
	delete streamNavigator;
	return false;
}

bool 
NfsV3PduDescriptor::getPathconf(Pathconf *pathconf)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, miscIndex0, pktDesc[0])) 
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->linkMax))
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->nameMax))
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->noTrunc))
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->chownRestrict))
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->caseInsensitive))
		goto err;
	if (!streamNavigator->getUint32Pdu(&pathconf->casePreserving))
		goto err;
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

bool
NfsV3PduDescriptor::getFileHandle(uint16_t index, PacketDescriptor *pktDesc)
{
	TcpStreamNavigator *streamNavigator = new TcpStreamNavigator();
	if (!streamNavigator->init(this, index, pktDesc))
		goto err;
	if (streamNavigator->getUint32Pdu(&fhLen)) {
		if (fhLen > NFS3_FHSIZE)
			goto err;
		if (streamNavigator->getBytesPdu(fileHandle, fhLen))
			return true;
	} else {
		delete streamNavigator;
		return false;
	}
	delete streamNavigator;
	return true;
err:
	delete streamNavigator;
	return false;
}

void
NfsV3PduDescriptor::print()
{
	printf("++++++++++++++++++++++++++\n");
	printf("NFSv3 ");
	switch(static_cast<int> (rpcPduType)) {
		case static_cast<int> (PDU_COMPLETE):
			printf("PDU_COMPLETE ");
			break;
		case static_cast<int> (PDU_COMPLETE_HEADER):
			printf("PDU_COMPLETE_HEADER ");
			break;
		case static_cast<int> (PDU_BAD):
			printf("PDU_BAD ");
			break;
	}
	switch(rpcProgramProcedure) {
		case NFS3PROC_NULL:
			printNULL();
			break;
		case NFS3PROC_GETATTR:
			printGETATTR();
			break;
		case NFS3PROC_SETATTR:
			printSETATTR();
			break;
		case NFS3PROC_LOOKUP:
			printLOOKUP();
			break;
		case NFS3PROC_ACCESS:
			printACCESS();
			break;
		case NFS3PROC_READLINK:
			printREADLINK();
			break;
		case NFS3PROC_READ:
			printREAD();
			break;
		case NFS3PROC_WRITE:
			printWRITE();
			break;
		case NFS3PROC_CREATE:
			printCREATE();
			break;		
		case NFS3PROC_MKDIR:
			printMKDIR();
			break;		
		case NFS3PROC_SYMLINK:
			printSYMLINK();
			break;	
		case NFS3PROC_MKNOD:
			printMKNOD();
			break;		
		case NFS3PROC_REMOVE:
			printREMOVE();
			break;		
		case NFS3PROC_RMDIR:
			printRMDIR();
			break;	
		case NFS3PROC_RENAME:
			printRENAME();
			break;		
		case NFS3PROC_LINK:
			printLINK();
			break;		
		case NFS3PROC_READDIR:
			printREADDIR();
			break;	
		case NFS3PROC_READDIRPLUS:
			printREADDIRPLUS();
			break;	
		case NFS3PROC_FSSTAT:
			printFSSTAT();
			break;		
		case NFS3PROC_FSINFO:
			printFSINFO();
			break;		
		case NFS3PROC_PATHCONF:
			printPATHCONF();
			break;	
		case NFS3PROC_COMMIT:
			printCOMMIT();
			break;
	}
	printf("xid:0x%08x pduLen:%u pkts:%d\n", rpcXid, rpcPduLen, getNumPackets());
	printf("pduBeginPktIndx:%u pduEndPktIndex:%u ", 
		firstPktDesc->packetBufferIndex, lastPktDesc->packetBufferIndex);
	printf("progBeginPktIndx:%u progBeginPktOffset:%u progEndPktIndx:%u "
		"progEndPktOffset:%u progEndPktRefCnt:%u\n", 
		firstPktDescRpcProg->packetBufferIndex,	rpcHeaderOffset, 
		lastPktDesc->packetBufferIndex, rpcProgramEndOffset,
		lastPktDesc->refCount.load());
}

void
NfsV3PduDescriptor::printNULL()
{
	printf("NULL ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			break;
	}
}

void
NfsV3PduDescriptor::printGETATTR()
{
	printf("GETATTR ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) 
				printFileHandle();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printSETATTR()
{
	printf("SETATTR ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) 
				printFileHandle();
			printSetAttr();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				printf("status:%u ", nfsStatus);
				printFileAttr();
			}
	}
}

void
NfsV3PduDescriptor::printLOOKUP()
{
	printf("LOOKUP ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
					// TODO: getting and printing dir attr
				} else {
					printf("status:%u\n", nfsStatus);
					printFileAttr(); 
				}
			}
	}
}

void
NfsV3PduDescriptor::printACCESS()
{
	printf("ACCESS ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("access:%#x\n", accessMode);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
					printf("access:%#x\n", accessMode);
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printREADLINK()
{
	printf("READLINK ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) 
				printFileHandle();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					unsigned char pathName[NFS_MAXPATHLEN + 1];
					if (!getFileName(pathName, miscIndex0, pktDesc[0])) {
						parsable = false;
						break;
					}
					printf("status:%u ", nfsStatus);
					printFileAttr();
					printf("path:%s\n", pathName);					
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printREAD()
{
	printf("READ ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("offset:%lu count:%u\n", fileOffset, 
						byteCount);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
					printf("count:%u data_idx:%u\n", byteCount, miscIndex0);
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printWRITE()
{
	printf("WRITE ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("offset:%lu count:%u stable:%u data_idx:%u\n",
					fileOffset, byteCount, writeStable, miscIndex0);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
					printf("count:%u stable:%u\n", byteCount, writeStable);
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printCREATE()
{
	printf("CREATE ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printFileHandle();
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printMKDIR()
{
	printf("MKDIR ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);
				printSetAttr();
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printFileHandle();
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printSYMLINK()
{
	printf("SYMLINK ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char linkName[NFS_MAXNAMLEN + 1];
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(linkName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				if (!getFileName(fileName, miscIndex1, pktDesc[1])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("link:%s\n", linkName);
				printSetAttr();
				printf("file:%s\n", fileName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printFileHandle();
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printMKNOD()
{
	printf("MKNOD ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char deviceName[NFS_MAXNAMLEN + 1];
				DeviceSpec spec;
				if (!getFileName(deviceName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("device:%s type:%u\n", deviceName, type);
				printSetAttr();
				if (type == NF3CHR || type == NF3BLK) {
					if (!getDeviceSpec(&spec)) {
						parsable = false;
						break;
					}
					printf("major:%u minor:%u\n", spec.majorNum, spec.minorNum);
				}
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printFileHandle();
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printREMOVE()
{
	printf("REMOVE ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printRMDIR()
{
	printf("RMDIR ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u ", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printRENAME()
{
	printf("RENAME ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				unsigned char fileName[NFS_MAXNAMLEN + 1];
				if (!getFileName(fileName, miscIndex0, pktDesc[0]))
					break;
				printFileHandle();
				printf("file:%s\n", fileName);
				if (!getFileName(fileName, miscIndex1, pktDesc[1])) {
					parsable = false;
					break;
				}				
				if (!getFileHandle(miscIndex2, pktDesc[2])) {
					parsable = false;
					break;
				}
				printFileHandle();
				printf("file:%s\n", fileName);				
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) 
				printf("status:%u \n", nfsStatus);
	}
}

void
NfsV3PduDescriptor::printLINK()
{
	printf("LINK ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				Link link;			
				if (!getLink(&link)) {
					parsable = false;
					break;
				}
				printFileHandle();
				// Ok for debug!
				memcpy(fileHandle, link.dirFileHandle, sizeof(fileHandle));
				printFileHandle();
				printf("link:%s\n", link.linkName);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				printf("status:%u ", nfsStatus);
				printFileAttr();				
			}
	}
}

void
NfsV3PduDescriptor::printREADDIR()
{
	printf("READDIR ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("count:%u\n", maxByteCount);	
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				printf("status:%u\n", nfsStatus);
				if (nfsStatus == NFS_OK) {
					unsigned char fileName[NFS_MAXNAMLEN + 1];
					bool eof = false;
					while (getReaddirEntry(fileName, eof) && !eof) {
						printf("file:%s fileid:%lu\n", fileName, fileId);
						printf("$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
					}
				}
			}
	}
}

void
NfsV3PduDescriptor::printREADDIRPLUS()
{
	printf("READDIRPLUS ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("dircount:%u maxcount:%u\n", byteCount, maxByteCount);	
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				printf("status:%u\n", nfsStatus);
				if (nfsStatus == NFS_OK) {
					unsigned char fileName[NFS_MAXNAMLEN + 1];
					bool eof = false;
					while (getReaddirplusEntry(fileName, eof) && !eof) {
						printf("file:%s\n", fileName);
						printFileAttr();
						printFileHandle();
						printf("$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
					}
				}
			}
	}
}

void
NfsV3PduDescriptor::printFSSTAT()
{
	printf("FSSTAT ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) 
				printFileHandle();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					Fsstat fsstat;
					if (!getFsstat(&fsstat)) {
						parsable = false;
						break;
					}
					printf("status:%u ", nfsStatus);
					printFileAttr();
					fsstat.print();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printFSINFO()
{
	printf("FSINFO ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) 
				printFileHandle();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					Fsinfo fsinfo;
					if (!getFsinfo(&fsinfo)) {
						parsable = false;
						break;
					}
					printf("status:%u ", nfsStatus);
					printFileAttr();
					fsinfo.print();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printPATHCONF()
{
	printf("PATHCONF ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable)
				printFileHandle();
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				printf("status:%u ", nfsStatus);
				printFileAttr();
				if (nfsStatus == NFS_OK) {
					Pathconf pathconf;
					if (!getPathconf(&pathconf)) {
						parsable = false;
						break;
					}		
					pathconf.print();
				}
			}
	}
}

void
NfsV3PduDescriptor::printCOMMIT()
{
	printf("COMMIT ");
	switch(rpcMsgType) {
		case RPC_CALL:
			printf("CALL\n");
			if (parsable) {
				printFileHandle();
				printf("offset:%lu count:%u\n", fileOffset, 
						byteCount);
			}
			break;
		case RPC_REPLY:
			printf("REPLY\n");
			if (parsable) {
				if (nfsStatus == NFS_OK) {
					printf("status:%u\n", nfsStatus);
					printFileAttr();
				} else
					printf("status:%u\n", nfsStatus);
			}
	}
}

void
NfsV3PduDescriptor::printFileHandle()
{
	printf("fh:");
	for (uint32_t i = 0; i < fhLen; i += 4) {
		uint32_t tmp = 0;
		for (int j = 0; j < 4; j++) {
			tmp <<= 8;
			tmp |= *(fileHandle + i + j);
		}
		printf("%08x", tmp);
	}
	printf("\n");
}

std::string
NfsV3PduDescriptor::getFileHandleStr()
{
	std::ostringstream fh;
	for (uint32_t i = 0; i < fhLen; i += 4) {
		std::ostringstream byte;
		byte.flags(std::ios_base::hex);
		byte.width(8);
		byte.fill('0');
		uint32_t tmp = 0;
		for (int j = 0; j < 4; j++) {
			tmp <<= 8;
			tmp |= *(fileHandle + i + j);
		}
		byte << tmp;
		fh << byte.str();
	}
	return fh.str();
}

void
NfsV3PduDescriptor::printFileAttr()
{
	printf("type:%u mode:%#x uid:%u gid:%u\n"
		"size:%lu used:%lu fsid:%#lx fileid:%lu mod:%#lx\n", 
		type, fileMode, fileUid, 
		fileGid, fileSizeBytes, fileUsedBytes, 
		fileSystemId, fileId, fileModTime);
}

void
NfsV3PduDescriptor::printSetAttr()
{
	printf("mode:%#x uid:%u gid:%u size:%lu mod:%#lx\n", 
		fileMode, fileUid, fileGid, fileSizeBytes, fileModTime);
}

BadPduDescriptor::BadPduDescriptor(PacketDescriptor *firstPktDesc, 
	PacketDescriptor *lastPktDesc)
	: PduDescriptor(firstPktDesc, lastPktDesc)
{
	rpcXid = 0; 
	rpcProgram = rpcProgramVersion = rpcProgramProcedure = rpcPduLen 
		= RPC_BAD_PDU; 
	next = NULL;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
	lastTouchingProcess = 0;
	#endif
}

void
BadPduDescriptor::print()
{
	PacketDescriptor *pktDesc; 
	printf("<<<<<<<<<<<<<<<<<<<<<<<<<<\nUNKNOWN BAD PDU\n");
	for (pktDesc = firstPktDesc; pktDesc != lastPktDesc;
			pktDesc = pktDesc->next)
		pktDesc->print();
	pktDesc->print();
	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>\n");
}
