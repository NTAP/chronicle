// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <cstdio>
#include "NfsParser.h"
#include "Message.h"
#include "ChronicleProcessRequest.h"
#include "RpcParser.h"
#include "TcpStreamNavigator.h"
#include "OutputModule.h"
#include <limits.h>

NfsParser::Nfs3OperationCounts NfsParser::nfs3OpCounts;

class NfsParser::MsgBase : public Message {
	protected:
		NfsParser *_parser;

	public:
		MsgBase(NfsParser *parser) : _parser(parser) { }
		virtual ~MsgBase() { }
};

class NfsParser::MsgProcessRequest : public MsgBase {
	private:
		PduDescriptor *_pduDesc;

	public:
		MsgProcessRequest(NfsParser *parser, PduDescriptor *pduDesc) 
			: MsgBase(parser), _pduDesc(pduDesc) { }
		void run() { _parser->doProcessRequest(_pduDesc); }
};

class NfsParser::MsgShutdownProcess : public MsgBase {
	public:
		MsgShutdownProcess(NfsParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doShutdownProcess(); }
};

class NfsParser::MsgKillNfsParser : public MsgBase {
	public:
		MsgKillNfsParser(NfsParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doKillNfsParser(); }
};

class NfsParser::MsgProcessDone : public MsgBase {
	public:
		MsgProcessDone(NfsParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doHandleProcessDone(); }
};

NfsParser::NfsParser(ChronicleSource *src, uint32_t pipelineId,
		OutputManager *outputManager) :
    Process("NfsParser"), _source(src), _pipelineId(pipelineId) 
{
	_outputManager = outputManager;
	_streamNavigator = new TcpStreamNavigator();
	_sink = NULL;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	_parsablePdus = _unparsablePdus = _failedPdus = 0;
	#endif
}

NfsParser::~NfsParser()
{
	delete _streamNavigator;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	printf("NfsParser::~NfsParser[%u]: parsablePdus:%lu unparsablePdus:%lu "
		"failedPdus:%lu\n", _pipelineId, _parsablePdus, _unparsablePdus, 
		_failedPdus);
	#endif
}

void
NfsParser::processRequest(ChronicleSource *src, PduDescriptor *pduDesc)
{
	enqueueMessage(new MsgProcessRequest(this, pduDesc));
}

void
NfsParser::doProcessRequest(PduDescriptor *pduDesc)
{
	PduDescriptor *tmpPduDesc = pduDesc;
	while (tmpPduDesc) {
		switch(tmpPduDesc->rpcProgram) {
			case NFS_PROGRAM:
				switch(tmpPduDesc->rpcProgramVersion) {
					case NFS3_VERSION:
						NfsV3PduDescriptor *nfsPduDesc = 
							static_cast<NfsV3PduDescriptor *> (tmpPduDesc);
						parseNfsV3Pdu(nfsPduDesc);
						#if CHRON_DEBUG(CHRONICLE_DEBUG_NFS)
						nfsPduDesc->print();
						#endif
						break;
				}
				break;
			case RPC_PORTMAP_PROGRAM:
				// TODO:
				break;
			//TODO: MOUNT_PROGRAM
			default:
				break;
		}
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		tmpPduDesc->tagPduPkts(PROCESS_NFS_PARSER);
		#endif
		tmpPduDesc = tmpPduDesc->next;
	}
	if (_sink)
		_sink->processRequest(this, pduDesc);
	else {
		PduDescReceiver *next;
		next = _outputManager->findNextModule(_pipelineId);
		next->processRequest(this, pduDesc);
	}
}

void
NfsParser::shutdown(ChronicleSource *src)
{
	if (src == _source)
		enqueueMessage(new MsgShutdownProcess(this));
}

void
NfsParser::doShutdownProcess()
{
	if (_sink)
		_sink->shutdown(this);
	else
		shutdownDone(NULL);
}

void
NfsParser::shutdownDone(ChronicleSink *next)
{
	if(next == _sink)
		enqueueMessage(new MsgKillNfsParser(this));
}

void
NfsParser::doKillNfsParser()
{
	_source->shutdownDone(this);
	exit();
}

void 
NfsParser::processDone(ChronicleSink *next)
{
	if (next == _sink) 
		enqueueMessage(new MsgProcessDone(this));
}

void
NfsParser::doHandleProcessDone()
{
	_source->processDone(this);
}

void
NfsParser::parseNfsV3Pdu(NfsV3PduDescriptor *nfsPduDesc)
{
	// Parsing successful RPC operations only
	if (nfsPduDesc->rpcAcceptState) {
		nfsPduDesc->parsable = false;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		_failedPdus++;
		#endif
		return ;
	}
	// Parsing complete PDUs only
	switch(static_cast<int> (nfsPduDesc->rpcPduType)) {
		case static_cast<int> (PduDescriptor::PDU_COMPLETE):
			break;
		case static_cast<int> (PduDescriptor::PDU_COMPLETE_HEADER):
		case static_cast<int> (PduDescriptor::PDU_BAD):
			nfsPduDesc->parsable = false;
			return ;
	}

	switch(nfsPduDesc->rpcProgramProcedure) {
		case NFS3PROC_NULL:
			nfsPduDesc->parsable = true;
			break;
		case NFS3PROC_GETATTR:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.getattr;
					if (parseNfs3GetattrCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3GetattrReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}
			break;
		case NFS3PROC_SETATTR:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.setattr;
					if (parseNfs3SetattrCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3SetattrReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_LOOKUP:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.lookup;
					if (parseNfs3LookupCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3LookupReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_ACCESS:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.access;
					if (parseNfs3AccessCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3AccessReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_READLINK:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.readlink;
					if (parseNfs3ReadlinkCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3ReadlinkReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_READ:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.read;
					if (parseNfs3ReadCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3ReadReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_WRITE:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.write;
					if (parseNfs3WriteCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3WriteReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_CREATE:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.create;
					if (parseNfs3CreateCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3CreateReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_MKDIR:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.mkdir;
					if (parseNfs3MkdirCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3MkdirReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_SYMLINK:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.symlink;
					if (parseNfs3SymlinkCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3SymlinkReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_MKNOD:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.mknod;
					if (parseNfs3MknodCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3MknodReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}			
			break;
		case NFS3PROC_REMOVE:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.remove;
					if (parseNfs3RemoveRmdirCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3RemoveRmdirReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;			
		case NFS3PROC_RMDIR:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.rmdir;
					if (parseNfs3RemoveRmdirCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3RemoveRmdirReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;	
		case NFS3PROC_RENAME:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.rename;
					if (parseNfs3RenameCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3RenameReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;	
		case NFS3PROC_LINK:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.link;
					if (parseNfs3LinkCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3LinkReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;	
		case NFS3PROC_READDIR:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.readdir;
					if (parseNfs3ReaddirCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3ReaddirReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;		
		case NFS3PROC_READDIRPLUS:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.readdirplus;
					if (parseNfs3ReaddirplusCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3ReaddirplusReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;		
		case NFS3PROC_FSSTAT:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.fsstat;
					if (parseNfs3FsstatCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3FsstatReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;
		case NFS3PROC_FSINFO:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.fsinfo;
					if (parseNfs3FsinfoCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3FsinfoReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;
		case NFS3PROC_PATHCONF:
				switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.pathconf;
					if (parseNfs3PathconfCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3PathconfReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;	
		case NFS3PROC_COMMIT:
			switch (nfsPduDesc->rpcMsgType) {
				case RPC_CALL:
					++nfs3OpCounts.commit;
					if (parseNfs3CommitCall(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
				case RPC_REPLY:
					if (parseNfs3CommitReply(nfsPduDesc))
						nfsPduDesc->parsable = true;
					break;
			}		
			break;
	}
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	if (nfsPduDesc->parsable)
		_parsablePdus++;
	else
		_unparsablePdus++;
	#endif
}

bool
NfsParser::parseNfs3GetattrCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return parseNfs3FileHandle(nfsPduDesc);
	return false;
}

bool
NfsParser::parseNfs3GetattrReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK)
		if (!parseNfs3FileAttr(nfsPduDesc))
				return false;
	return true;
}

bool
NfsParser::parseNfs3SetattrCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!parseNfs3SetAttr(nfsPduDesc))
		return false;
	// ignoring the rest
	return true;	
}

bool
NfsParser::parseNfs3SetattrReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (!parseNfs3PrePostOpAttrs(nfsPduDesc))
		return false;
	return true;
}

bool
NfsParser::parseNfs3LookupCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	return true;
}

bool
NfsParser::parseNfs3LookupReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3FileHandle(nfsPduDesc))
			return false;
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
		// TODO: parsing dir attributes
	} else {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	return true;
}

bool
NfsParser::parseNfs3AccessCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->accessMode))
		return false;
	return true;
}

bool
NfsParser::parseNfs3AccessReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->accessMode))
			return false;
	} 
	return true;
}

bool
NfsParser::parseNfs3ReadlinkCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return parseNfs3FileHandle(nfsPduDesc);
	return false;
}

bool
NfsParser::parseNfs3ReadlinkReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
		nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
		nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	} else {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	return true;
}

bool
NfsParser::parseNfs3ReadCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileOffset))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
		return false;
	return true;
}

bool
NfsParser::parseNfs3ReadReply(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t attrFollows = 0;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!_streamNavigator->getUint32Pdu(&attrFollows))
			return false;
		if (attrFollows == 1) 
			if (!parseNfs3FileAttr(nfsPduDesc))
				return false;
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
			return false;
		if (!_streamNavigator->skipBytesPdu(4))
			return false;
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
			return false;
		nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
		nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();	
	} 
	return true;
}

bool
NfsParser::parseNfs3WriteCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileOffset))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->writeStable))
		return false;	
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();	
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();	
	return true;
}

bool
NfsParser::parseNfs3WriteReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PrePostOpAttrs(nfsPduDesc))
			return false;
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
			return false;
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->writeStable))
			return false;
		// ignoring the rest
	}
	return true;
}

bool
NfsParser::parseNfs3CreateCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	// ignoring the rest
	return true; 
}

bool
NfsParser::parseNfs3CreateReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpFileHandle(nfsPduDesc))
			return false;
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	// TODO: parsing dir attributes
	return true;
}

bool
NfsParser::parseNfs3MkdirCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	if (!parseNfs3SetAttr(nfsPduDesc))
		return false;
	return true; 
}

bool
NfsParser::parseNfs3MkdirReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpFileHandle(nfsPduDesc))
			return false;
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	// TODO: parsing dir attributes
	return true;
}

bool
NfsParser::parseNfs3SymlinkCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	if (!parseNfs3SetAttr(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex1 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[1] = _streamNavigator->getPacketDesc();
	return true;
}

bool
NfsParser::parseNfs3SymlinkReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpFileHandle(nfsPduDesc))
			return false;
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	// TODO: parsing dir attributes
	return true;
}

bool
NfsParser::parseNfs3MknodCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->type))
		return false;
	switch(nfsPduDesc->type) {
		case NF3CHR:
		case NF3BLK:
			if (!parseNfs3SetAttr(nfsPduDesc))
				return false;
			nfsPduDesc->miscIndex1 = _streamNavigator->getIndex();	
			nfsPduDesc->pktDesc[1] = _streamNavigator->getPacketDesc();
			break;
		case NF3SOCK:
		case NF3FIFO:
			if (!parseNfs3SetAttr(nfsPduDesc))
				return false;
			break;
	}
	return true;
}

bool
NfsParser::parseNfs3MknodReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpFileHandle(nfsPduDesc))
			return false;
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	// ignoring the rest
	return true;
}

bool
NfsParser::parseNfs3RemoveRmdirCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	return true; 
}

bool
NfsParser::parseNfs3RemoveRmdirReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	if (nfsPduDesc->nfsStatus == NFS_OK) 
		if (!parseNfs3PrePostOpAttrs(nfsPduDesc))
			return false;
	return true;
}

bool
NfsParser::parseNfs3RenameCall(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fileNameLen, residual;
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	nfsPduDesc->miscIndex2 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[2] = _streamNavigator->getPacketDesc();
	if (!skipNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex1 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[1] = _streamNavigator->getPacketDesc();
	if (!_streamNavigator->getUint32Pdu(&fileNameLen))
		return false;
	if (fileNameLen > NFS3_MAXNAMLEN)
		return false;
	residual = fileNameLen & 0x3;
	if (residual)
		fileNameLen += 4 - residual;
	if (!_streamNavigator->skipBytesPdu(fileNameLen))
		return false;
	return true;
}

bool
NfsParser::parseNfs3RenameReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;	
	// ignoring the rest
	return true;
}

bool
NfsParser::parseNfs3LinkCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	// parsing the rest later (see getLink() in ChronicleProcessRequest.cc)
	return true;
}

bool
NfsParser::parseNfs3LinkReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (!parseNfs3PostOpAttr(nfsPduDesc))
		return false;
	// ignoring the rest
	return true;
}

bool
NfsParser::parseNfs3ReaddirCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->skipBytesPdu(8 + NFS3_COOKIEVERFSIZE))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->maxByteCount))
		return false;
	return true;
}

bool
NfsParser::parseNfs3ReaddirReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
		if (!_streamNavigator->skipBytesPdu(NFS3_COOKIEVERFSIZE))
			return false;
		nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
		nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
		// parsing the rest later (see getReaddirEntry() in ChronicleProcessRequest.cc)
	} else {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	return true;
}

bool
NfsParser::parseNfs3ReaddirplusCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->skipBytesPdu(8 + NFS3_COOKIEVERFSIZE))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->maxByteCount))
		return false;
	return true;
}

bool
NfsParser::parseNfs3ReaddirplusReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
		if (!_streamNavigator->skipBytesPdu(NFS3_COOKIEVERFSIZE))
			return false;
		nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
		nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
		// parsing the rest later (see getReaddirplusEntry() in ChronicleProcessRequest.cc)
	} else {
		if (!parseNfs3PostOpAttr(nfsPduDesc))
			return false;
	}
	return true;
}

bool
NfsParser::parseNfs3FsstatCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	return true;
}

bool
NfsParser::parseNfs3FsstatReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (!parseNfs3PostOpAttr(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	// parsing the rest later (see getFsstat() in ChronicleProcessRequest.cc)
	return true;
}

bool
NfsParser::parseNfs3FsinfoCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	return true;
}

bool
NfsParser::parseNfs3FsinfoReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (!parseNfs3PostOpAttr(nfsPduDesc))
		return false;
	nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
	nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	// parsing the rest later (see getFsinfo() in ChronicleProcessRequest.cc)
	return true;
}

bool
NfsParser::parseNfs3PathconfCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return parseNfs3FileHandle(nfsPduDesc);
	return false;
}

bool
NfsParser::parseNfs3PathconfReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (!parseNfs3PostOpAttr(nfsPduDesc))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		nfsPduDesc->miscIndex0 = _streamNavigator->getIndex();
		nfsPduDesc->pktDesc[0] = _streamNavigator->getPacketDesc();
	}
	return true;
}

bool
NfsParser::parseNfs3CommitCall(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset))
		return false;
	if (!parseNfs3FileHandle(nfsPduDesc))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileOffset))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->byteCount))
		return false;
	return true;
}

bool
NfsParser::parseNfs3CommitReply(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->init(nfsPduDesc, nfsPduDesc->rpcProgramStartOffset)) 
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->nfsStatus))
		return false;
	if (nfsPduDesc->nfsStatus == NFS_OK) {
		if (!parseNfs3PrePostOpAttrs(nfsPduDesc))
			return false;
		// ignoring the rest
	}
	return true;
}

bool
NfsParser::parseNfs3FileHandle(NfsV3PduDescriptor *nfsPduDesc)
{
	if (_streamNavigator->getUint32Pdu(&nfsPduDesc->fhLen)) {
		if (nfsPduDesc->fhLen > NFS3_FHSIZE)
			return false;
		if (_streamNavigator->getBytesPdu(&nfsPduDesc->fileHandle[0], 
				nfsPduDesc->fhLen))
			return true;
	}
	return false;
}

bool
NfsParser::skipNfs3FileHandle(NfsV3PduDescriptor *nfsPduDesc)
{
	if (_streamNavigator->getUint32Pdu(&nfsPduDesc->fhLen)) {
		if (nfsPduDesc->fhLen > NFS3_FHSIZE)
			return false;
		if (_streamNavigator->skipBytesPdu(nfsPduDesc->fhLen))
			return true;
	}
	return false;
}

bool
NfsParser::parseNfs3FileAttr(NfsV3PduDescriptor *nfsPduDesc)
{
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->type))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileMode))
		return false;
	else 
		nfsPduDesc->fileMode &= 0xfff;
	if (!_streamNavigator->skipBytesPdu(4)) // ignoring nlink
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileUid))
		return false;
	if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileGid))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileSizeBytes))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileUsedBytes))
		return false;
	if (!_streamNavigator->skipBytesPdu(8)) // ignoring rdev
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileSystemId))
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileId))
		return false;
	if (!_streamNavigator->skipBytesPdu(8)) // ignoring access time
		return false;
	if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileModTime))
		return false;
	if (!_streamNavigator->skipBytesPdu(8)) // ignoring ctime
		return false;
	return true;
}

bool
NfsParser::parseNfs3SetAttr(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t follows;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;
	if (follows == 1) {
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileMode))
			return false;
	} else 
		nfsPduDesc->fileMode = 0;
	nfsPduDesc->fileMode &= 0xfff;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;
	if (follows == 1) {
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileUid))
			return false;
	} else
		nfsPduDesc->fileUid = 0;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;
	if (follows == 1) {
		if (!_streamNavigator->getUint32Pdu(&nfsPduDesc->fileGid))
			return false;
	} else
		nfsPduDesc->fileGid = 0;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;
	if (follows == 1) {
		if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileSizeBytes))
			return false;
	} else
		nfsPduDesc->fileSizeBytes = 0;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;
	if (follows == NFS3_SET_TO_CLIENT_TIME
			&& !_streamNavigator->skipBytesPdu(8)) // ignoring access time
		return false;
	if (!_streamNavigator->getUint32Pdu(&follows))
		return false;	
	if (follows == NFS3_SET_TO_CLIENT_TIME) {
		if (!_streamNavigator->getUint64Pdu(&nfsPduDesc->fileModTime))
			return false;
	} else
		nfsPduDesc->fileModTime = 0;
	// ignoring the rest
	return true;
}

bool
NfsParser::parseNfs3PostOpFileHandle(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t fhFollows;	
	if (!_streamNavigator->getUint32Pdu(&fhFollows))
		return false;
	if (fhFollows == 1)
		if (!parseNfs3FileHandle(nfsPduDesc))
			return false;
	return true;
}

bool
NfsParser::parseNfs3PostOpAttr(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t attrFollows;	
	if (!_streamNavigator->getUint32Pdu(&attrFollows))
		return false;
	if (attrFollows == 1)
		if (!parseNfs3FileAttr(nfsPduDesc))
			return false;
	return true;
}

bool
NfsParser::parseNfs3PrePostOpAttrs(NfsV3PduDescriptor *nfsPduDesc)
{
	uint32_t attrFollows;
	if (!_streamNavigator->getUint32Pdu(&attrFollows))
		return false;
	if (attrFollows == 1) // skipping pre op attrs
		if (!_streamNavigator->skipBytesPdu(24))
			return false;
	if (!parseNfs3PostOpAttr(nfsPduDesc))
		return false;
	return true;
}

