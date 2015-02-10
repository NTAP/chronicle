// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include <netinet/in.h>
#include <iterator>
#include <sys/time.h>
#include <cstdio>
#include <errno.h>
#include "RpcParser.h"
#include "Message.h"
#include "ChronicleProcessRequest.h"
#include "ChronicleConfig.h"
#include "PacketBuffer.h"
#include "PcapPacketBufferPool.h"	
#include "FlowTable.h"
#include "NfsParser.h"
#include "TcpStreamNavigator.h"

#define MATCH_SIGN0_CALL(num)												\
	(num == 0x0 || num == 0x00000002 || num == 0x00000200 					\
	|| num == 0x00020001 || num == 0x02000186)
#define MATCH_SIGN1_CALL(num)												\
	(num == 0x00000200 || num == 0x00020001 || num == 0x02000186			\
	|| num == 0x000186a3 || num == 0x0186a300 || num == 0x86a30000			\
	|| num == 0xa3000000)
#define MATCH_SIGN0_REPLY(num)												\
	(num == 0x0 || num == 0x01000000 || num == 0x00010000					\
	|| num == 0x00000100 || num == 0x1)

FlowDescriptorTcpRpc::FlowDescriptorTcpRpc(uint32_t sourceIP, uint32_t destIP,
	uint16_t sourcePort, uint16_t destPort, uint8_t protocol, RpcParser *parser)
	: FlowDescriptorTcp(sourceIP, destIP, sourcePort, destPort, protocol), _rpcParser(parser) 
{ 
	_readyPdusListHead = _readyPdusListTail = NULL;
	_readyPdusListSize = _unmatchedCallCount = 0;
	_callPduListByInsert = &get<0>(_callPduList);
	_callPduListByXid = &get<1>(_callPduList);
}

FlowDescriptorTcpRpc::~FlowDescriptorTcpRpc()
{
	assert(_callPduList.size() == 0 && _readyPdusListSize == 0);
	assert(_readyPdusListHead == NULL && _readyPdusListTail == NULL);
}

void
FlowDescriptorTcpRpc::insertCallPdu(PduDescriptor *pduDesc, bool shutdown) 
{
	_callPduList.push_back(pduDesc);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	std::cout << "insertCallPdu: _callPduList.size(): " << _callPduList.size()
		<< std::endl;
	#endif
	// garbage collecting unmatched calls
	if (!shutdown) {
		size_t size = _callPduList.size();
		PduListByInsert::iterator it = _callPduListByInsert->begin();

		// 2 levels of garbage collection
		if (size > (RPC_PARSER_UMATCH_CALL_GC_THRESH << 1)) {
			// (i) garbage collection by count
			_unmatchedCallCount++;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
			printf("GC: unmatched call:%u pduDesc->rpcXid:%#010x "
				"(*it)->rpcXid:%#010x callPdusListSize:%lu\n", 
				_unmatchedCallCount, pduDesc->rpcXid,
				(*it)->rpcXid, size);
			(*it)->print();
			#endif
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			_rpcParser->incrementUnmatchedCalls(1);
			#endif
			insertReadyPdus(*it, 1);
			_callPduListByInsert->pop_front();
		}
		if (size > RPC_PARSER_UMATCH_CALL_GC_THRESH) {
			// (ii) garbage collection by time
			gcCallPdus(pduDesc->firstPktDesc);
		}
	}
}

PduDescriptor *
FlowDescriptorTcpRpc::getCallPdu(uint32_t xid)
{
	PduListByXid::iterator it = _callPduListByXid->find(xid);
	if (it == _callPduListByXid->end())
		return NULL;
	else {
		PduDescriptor *pduDesc = *it;
		_callPduListByXid->erase(it);
		return pduDesc;
	}
}

bool
FlowDescriptorTcpRpc::hasSeenCallPdu(uint32_t xid)
{
	PduListByXid::iterator it = _callPduListByXid->find(xid);
	return !(it == _callPduListByXid->end());
}

void
FlowDescriptorTcpRpc::insertReadyPdus(PduDescriptor *pduDesc, uint16_t num)
{
	// insert PDUs in an RPC call/reply pair or an unmatched call by itself
	_readyPdusListSize += num;
	if (_readyPdusListTail == NULL) 
		_readyPdusListHead = _readyPdusListTail = pduDesc;
	else {
		_readyPdusListTail->next = pduDesc;
		_readyPdusListTail = pduDesc;
	}
	while ((pduDesc = pduDesc->next) != NULL)
		_readyPdusListTail = pduDesc;
	_readyPdusListTail->next = NULL;	
}

void
FlowDescriptorTcpRpc::passReadyPdus(PduDescReceiver *sink, RpcParser *src)
{
	if (_readyPdusListHead != NULL) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
		PduDescriptor *tmpPduDesc = _readyPdusListHead;
		while (tmpPduDesc != NULL) {
			tmpPduDesc->tagPduPkts(PROCESS_RPC_PARSER);
			tmpPduDesc = tmpPduDesc->next;
		}
		#endif
		sink->processRequest(src, _readyPdusListHead);
		_readyPdusListHead = _readyPdusListTail = NULL;
		_readyPdusListSize = 0;
	}
}

void
FlowDescriptorTcpRpc::passCallPdus()
{
	PduListByInsert::iterator it = _callPduListByInsert->begin();
	while (it != _callPduListByInsert->end()) {
		++_unmatchedCallCount;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
		printf("GC: unmatched call:%u pduDesc->rpcXid:%#010x "
			"(*it)->rpcXid:%#010x callPdusListSize:%lu\n", 
			_unmatchedCallCount, (*it)->rpcXid,
			(*it)->rpcXid, _callPduListByInsert->size());
		(*it)->print();
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		_rpcParser->incrementUnmatchedCalls(1);
		#endif
		insertReadyPdus(*it, 1);
		it = _callPduListByInsert->erase(it);
	}	
}

void
FlowDescriptorTcpRpc::gcCallPdus(PacketDescriptor *pktDesc)
{
	PduListByInsert::iterator it = _callPduListByInsert->begin();
	while (it != _callPduListByInsert->end()) {
		long interval = (long)(pktDesc->pcapHeader.ts.tv_sec) -
			(long)((*it)->firstPktDesc->pcapHeader.ts.tv_sec);
		if (interval <= FLOW_DESC_TIME_WINDOW)
			break;
		++_unmatchedCallCount;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
		printf("GC: unmatched call:%u pduDesc->rpcXid:%#010x "
			"(*it)->rpcXid:%#010x callPdusListSize:%lu\n", 
			_unmatchedCallCount, (*it)->rpcXid,
			(*it)->rpcXid, _callPduListByInsert->size());
		(*it)->print();
		#endif
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		_rpcParser->incrementUnmatchedCalls(1);
		#endif
		insertReadyPdus(*it, 1);
		it = _callPduListByInsert->erase(it);
	}	
}

void
FlowDescriptorTcpRpc::increaseVisitCount(uint32_t numPkts, uint16_t newCount)
{
	uint32_t i;
	PacketDescriptor *pktDesc = head;
	for (i = 0; pktDesc != NULL && i < numPkts; i++) {
		if (pktDesc->visitCount < newCount)
			pktDesc->visitCount = newCount;
		else
			break;
		pktDesc = pktDesc->next;
	}
}

void
FlowDescriptorTcpRpc::increaseVisitCount(PacketDescriptor *pktDesc, 
	uint16_t newCount)
{
	PacketDescriptor *tmpPktDesc = head;
	if (pktDesc == NULL)
		return ;
	while (tmpPktDesc && tmpPktDesc != pktDesc) {
		if (tmpPktDesc->visitCount < newCount)
			tmpPktDesc->visitCount = newCount;
		tmpPktDesc = tmpPktDesc->next;
	}
	if (tmpPktDesc == pktDesc)
		if (tmpPktDesc->visitCount < newCount)
			tmpPktDesc->visitCount = newCount;
}

void
FlowDescriptorTcpRpc::printStats()
{
	FlowDescriptorTcp *flowDescTcp = this;
	flowDescTcp->printStats();
	printf("unmatchedCallCount:%lu\n", _unmatchedCallCount);
}

class RpcParser::MsgBase : public Message {
	protected:
		RpcParser *_parser;

	public:
		MsgBase(RpcParser *parser) : _parser(parser) { }
		virtual ~MsgBase() { }
};

class RpcParser::MsgProcessRequest : public MsgBase {
	private:
		PacketDescriptor *_pktDesc;

	public:
		MsgProcessRequest(RpcParser *parser, PacketDescriptor *pktDesc) 
			: MsgBase(parser), _pktDesc(pktDesc) { }
		void run() { _parser->doProcessRequest(_pktDesc); }
};

class RpcParser::MsgShutdownProcess : public MsgBase {
	public:
		MsgShutdownProcess(RpcParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doShutdownProcess(); }
};

class RpcParser::MsgKillRpcParser : public MsgBase {
	public:
		MsgKillRpcParser(RpcParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doKillRpcParser(); }
};

class RpcParser::MsgProcessDone : public MsgBase {
	public:
		MsgProcessDone(RpcParser *parser) 
			: MsgBase(parser) { }
		void run() { _parser->doHandleProcessDone(); }
};

RpcParser::RpcParser(ChronicleSource *src, unsigned pipelineId) 
	: Process("RpcParser"), _source(src), _pipelineId(pipelineId)
{
	struct timeval now;
	_bufPool = PcapPacketBufferPool::registerBufferPool();
	if (_bufPool == NULL) {
		_processState = ChronicleSink::CHRONICLE_ERR;
		_source->processDone(this);	
	}
	_flowTable = new FlowTable();
	_streamNavigator  = new TcpStreamNavigator();
	if (!gettimeofday(&now, NULL)) {
		_gcTick = now.tv_sec * 1000000UL + now.tv_usec;
		_gcBucket = 0;
	} else {
		perror("RpcParser::RpcParser: gettimeofday");
		_processState = ChronicleSink::CHRONICLE_ERR;
		_source->processDone(this);
	}
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	_goodPduCount = _badPduCount = _completePduCallCount = _completePduReplyCount
		= _completeHdrPduCallCount = _completeHdrPduReplyCount = 0;
	_unmatchedCallCount = _unmatchedReplyCount = 0;
	_goodPduPrintLimit = _badPduPrintLimit = 100000;
	_scannedRpcCallHdrs = _scannedRpcReplyHdrs = 0;
	_forcedRpcReplyScanCount = _forcedGcCount = 0;
	#endif
}

RpcParser::~RpcParser()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	printf("RpcParser::~RpcParser[%u]: goodPduCount:%lu badPduCount:%lu "
		"completePduCallCount:%lu completePduReplyCount:%lu "
		"completeHdrPduCallCount:%lu completeHdrPduReplyCount:%lu "
		"unmatchedCallCount:%lu unmatchedReplyCount:%lu "
		"scannedRpcCallHdrs:%lu scannedRpcReplyHdrs:%lu "
		"forcedRpcReplyScanCount:%lu forcedGarbageCollectCount:%lu\n",
		_pipelineId, _goodPduCount, _badPduCount, 
		_completePduCallCount, _completePduReplyCount,
		_completeHdrPduCallCount, _completeHdrPduReplyCount,
		_unmatchedCallCount, _unmatchedReplyCount,
		_scannedRpcCallHdrs, _scannedRpcReplyHdrs, _forcedRpcReplyScanCount,
		_forcedGcCount);
	if(_goodPduCount != _completePduCallCount + _completePduReplyCount +
			_completeHdrPduCallCount + _completeHdrPduReplyCount)
		printf("RpcParser::~RpcParser[%u]: [WARNING] stats do not add up!\n",
			_pipelineId);
	#endif
	delete _flowTable;
	delete _streamNavigator;
	if (_bufPool && _bufPool->unregisterBufferPool()) 
		delete _bufPool;
}

void
RpcParser::processRequest(ChronicleSource *src, 
	PacketDescriptor *pktDesc)
{
	enqueueMessage(new MsgProcessRequest(this, pktDesc));
}

void
RpcParser::doProcessRequest(PacketDescriptor *pktDesc)
{
	FlowDescriptorIPv4 *flowDesc; 
	unsigned bucketNum;
	bool forceFlush = false;
	FlowDescriptorTcpRpc *rpcFlowDesc = NULL, *rpcReverseFlowDesc = NULL;

	if (_processState == ChronicleSink::CHRONICLE_ERR 
			|| !isRpcConnection(pktDesc)) { // discarding non-RPC packets
		assert(_bufPool->releasePacketDescriptor(pktDesc));
		return ;
	}
	flowDesc = _flowTable->lookupFlow(pktDesc->srcIP, pktDesc->destIP, 
		pktDesc->srcPort, pktDesc->destPort, pktDesc->protocol, 
		bucketNum);
	if (pktDesc->protocol == IPPROTO_TCP) { // TCP packet
		if (flowDesc == NULL) {
			rpcFlowDesc = new FlowDescriptorTcpRpc(
				pktDesc->srcIP, pktDesc->destIP,
				pktDesc->srcPort, pktDesc->destPort, IPPROTO_TCP, this);
			_flowTable->insertFlow(rpcFlowDesc, bucketNum);
			// Since it's TCP, add the flow in the reverse direcection 
			if (_flowTable->lookupFlow(pktDesc->destIP, 
					pktDesc->srcIP, pktDesc->destPort, pktDesc->srcPort,
					IPPROTO_TCP, bucketNum) == NULL) {
				rpcReverseFlowDesc = new FlowDescriptorTcpRpc(
					pktDesc->destIP, pktDesc->srcIP,
					pktDesc->destPort, pktDesc->srcPort, IPPROTO_TCP, this);
				_flowTable->insertFlow(rpcReverseFlowDesc, bucketNum);
				rpcFlowDesc->flowDescriptorReverse = rpcReverseFlowDesc;
				rpcReverseFlowDesc->flowDescriptorReverse = rpcFlowDesc;
				#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
				assert(rpcFlowDesc == rpcReverseFlowDesc->flowDescriptorReverse 
					&& rpcReverseFlowDesc == rpcFlowDesc->flowDescriptorReverse);
				#endif
			}
		} else {
			rpcFlowDesc = static_cast<FlowDescriptorTcpRpc *> (flowDesc);
			rpcReverseFlowDesc = static_cast<FlowDescriptorTcpRpc *> 
				(rpcFlowDesc->flowDescriptorReverse);
			#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
			assert(rpcFlowDesc == rpcReverseFlowDesc->flowDescriptorReverse 
				&& rpcReverseFlowDesc == rpcFlowDesc->flowDescriptorReverse);
			#endif
		}
		
		if (rpcFlowDesc->insertPacketDescriptor(pktDesc, forceFlush)) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
			bool reverse = false;
			if(!rpcFlowDesc->isValid(reverse)) {
				if (!reverse)
					printf("insertPacketDescriptor: invalid forward");
				else
					printf("insertPacketDescriptor: invalid reverse");
				printf(" after inserting pktDesc->index:%u\n", 
					pktDesc->packetBufferIndex);
				pktDesc->print();
				rpcFlowDesc->print();
				rpcFlowDesc->printReverse();
				assert(false);
			}
			#endif 
			#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
			pktDesc->lastTouchingProcess = PROCESS_NET_PARSER;
			#endif
			if (rpcFlowDesc->queueLen >= RPC_PARSER_PACKETS_BATCH_SIZE 
					|| forceFlush) {
				parseRpcPackets(rpcFlowDesc, rpcReverseFlowDesc);
			}			
			// time-based garbage collection of the flow table buckets
			gcFlowTable(pktDesc);
		} else {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			rpcFlowDesc->droppedPktCount++;
			#endif
			assert(_bufPool->releasePacketDescriptor(pktDesc));
		}
	} else if (pktDesc->protocol == IPPROTO_UDP) { // UDP packet
		// TODO: handling UDP packets	
		assert(_bufPool->releasePacketDescriptor(pktDesc));
	} else // releasing pktDesc
		assert(_bufPool->releasePacketDescriptor(pktDesc));
}

void
RpcParser::shutdown(ChronicleSource *src)
{
	if (src == _source)
		enqueueMessage(new MsgShutdownProcess(this));
}

void
RpcParser::doShutdownProcess()
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	printf("======== CLEAN UP ========\n");
	#endif
	// converting all oustanding packets into pdus and passing them
	FlowDescriptorIPv4 *flowDesc = _flowTable->getAllFlows();
	FlowDescriptorIPv4 *firstFlowDesc = flowDesc;
	while (flowDesc) {
		if (flowDesc->head != NULL 
				|| flowDesc->flowDescriptorReverse->head != NULL) {
			PduDescriptor *pduDesc = NULL;
			PacketDescriptor *pktDesc = flowDesc->head;
			if (pktDesc == NULL)
				pktDesc = flowDesc->flowDescriptorReverse->head;
			if (pktDesc->protocol == IPPROTO_TCP) {
				FlowDescriptorTcpRpc *callFlowDesc = NULL, 
					*replyFlowDesc = NULL;  
				if (flowDesc->destPort == NFS_PORT 
						|| flowDesc->destPort == SUNRPC_PORT) {
					callFlowDesc = 
						static_cast<FlowDescriptorTcpRpc *> (flowDesc);
					replyFlowDesc = static_cast<FlowDescriptorTcpRpc *> 
						(flowDesc->flowDescriptorReverse);
					#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
					assert(callFlowDesc == replyFlowDesc->flowDescriptorReverse
						&& replyFlowDesc == 
						callFlowDesc->flowDescriptorReverse);
					#endif
				} else {
					callFlowDesc = static_cast<FlowDescriptorTcpRpc *> 
						(flowDesc->flowDescriptorReverse);
					replyFlowDesc = 
						static_cast<FlowDescriptorTcpRpc *> (flowDesc);
					#if CHRON_DEBUG(CHRONICLE_DEBUG_FLOWTABLE)
					assert(callFlowDesc == replyFlowDesc->flowDescriptorReverse
						&& replyFlowDesc == 
						callFlowDesc->flowDescriptorReverse);
					#endif
				}

				if (callFlowDesc->head) { // first processing calls
					PacketDescriptor *startPktDesc = NULL, *retryPktDesc = NULL;
					bool retry;
					do { // find new call pdus
						CompletePduAction action = ACTION_NONE;
						retry = false;
						pduDesc = findPdu(callFlowDesc, action,
							&startPktDesc);
						if (pduDesc != NULL) {
							callFlowDesc->insertCallPdu(pduDesc, true);
							if (startPktDesc == NULL)
								break;
						}
						else if (scanForRpcHeader(callFlowDesc, 
								&startPktDesc)) {
							if (startPktDesc != retryPktDesc) {
								retry = true;
								retryPktDesc = startPktDesc;
							}
							// shortcut in garbage collection
							callFlowDesc->increaseVisitCount(startPktDesc,
								RPC_PARSER_GC_THRESH);
						}
					} while ((pduDesc != NULL && callFlowDesc->head != NULL)
						|| retry);
					if (callFlowDesc->head != NULL) { // handling bad packets
						pduDesc = constructBadPdu(callFlowDesc->head, 
							callFlowDesc->tail);
						if (pduDesc != NULL) {
							callFlowDesc->releasePktDescriptors(
								pduDesc->lastPktDesc, true);
							passBadPdu(pduDesc);
						}
					}
				}

				if (replyFlowDesc->head) { // second processing replies
					PacketDescriptor *startPktDesc = NULL, *retryPktDesc = NULL;
					bool retry;
					do { // find new reply pdus
						CompletePduAction action = ACTION_NONE;
						retry = false;
						pduDesc = findPdu(replyFlowDesc, action, 
							&startPktDesc);
						if (pduDesc != NULL) {
							callFlowDesc->insertReadyPdus(pduDesc, 2); 
							if (startPktDesc == NULL)
								break;
						}
						else if (scanForRpcHeader(replyFlowDesc, 
								&startPktDesc)) {
							if (startPktDesc != retryPktDesc) {
								retry = true;
								retryPktDesc = startPktDesc;
							}
							// shortcut in garbage collection
							callFlowDesc->increaseVisitCount(startPktDesc,
								RPC_PARSER_GC_THRESH);					
						}
					} while ((pduDesc != NULL && replyFlowDesc->head != NULL)
						|| retry);
					if (replyFlowDesc->head != NULL) { // handling bad packets
						pduDesc = constructBadPdu(replyFlowDesc->head, 
							replyFlowDesc->tail);
						if (pduDesc != NULL) {
							replyFlowDesc->releasePktDescriptors(
								pduDesc->lastPktDesc, true);
							passBadPdu(pduDesc);
						}
					}
				}

				assert(callFlowDesc->head == NULL 
					&& replyFlowDesc->head == NULL);
			}
			else if (flowDesc->head->protocol == IPPROTO_UDP) {
				// TODO: handling UDP
			}
		}
		flowDesc = flowDesc->next;
	}
	flowDesc = firstFlowDesc;
	while (flowDesc) {
		// pass all outstanding PDUs
		FlowDescriptorTcpRpc *rpcFlowDesc = 
			static_cast<FlowDescriptorTcpRpc *>(flowDesc);
		rpcFlowDesc->passCallPdus();
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		_goodPduCount += rpcFlowDesc->_readyPdusListSize;
		rpcFlowDesc->printStats();
		#endif
		rpcFlowDesc->passReadyPdus(_sink, this);
		flowDesc = flowDesc->next;
		delete rpcFlowDesc;
	}
	_sink->shutdown(this);
}

void
RpcParser::shutdownDone(ChronicleSink *next)
{
	if(next == _sink)
		enqueueMessage(new MsgKillRpcParser(this));
}

void
RpcParser::doKillRpcParser()
{
	_source->shutdownDone(this);
	exit();
}

void 
RpcParser::processDone(ChronicleSink *next)
{
	if (next == _sink) 
		enqueueMessage(new MsgProcessDone(this));
}

void
RpcParser::doHandleProcessDone()
{
	_processState = ChronicleSink::CHRONICLE_ERR;
	_source->processDone(this);
}

void
RpcParser::parseRpcPackets(FlowDescriptorTcpRpc *rpcFlowDesc,
	FlowDescriptorTcpRpc *rpcReverseFlowDesc)
{
	PduDescriptor *pduDesc;
	PacketDescriptor *startPktDesc = NULL;
	do {
		CompletePduAction action = ACTION_NONE;
		pduDesc = findPdu(rpcFlowDesc, action, &startPktDesc);
		if (pduDesc != NULL && action == ACTION_INSERT_CALL) {
			rpcFlowDesc->insertCallPdu(pduDesc);
			if (startPktDesc == NULL) // examined all the packets
				break;
		}
		else if (pduDesc != NULL && action == ACTION_PASS_REPLY) {
			// sending a call/reply pdu pair at a time
			rpcReverseFlowDesc->insertReadyPdus(pduDesc, 2);
			if (rpcReverseFlowDesc->_readyPdusListSize >= 
					RPC_PARSER_PDUS_BATCH_SIZE) {
				#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
				_goodPduCount += rpcReverseFlowDesc->_readyPdusListSize;
				if (_goodPduCount >= _goodPduPrintLimit) {
					printf("RpcParser::doProcessRequest[%u]: "
						"goodPduCount:%lu badPduCount:%lu "
						"CC:%lu CR:%lu CHC:%lu CHR:%lu "
						"unmatchedCallCount:%lu unmatchedReplyCount:%lu "
						"scannedRpcCallHdrs:%lu scannedRpcReplyHdrs:%lu "
						"forcedRpcReplyScanCount:%lu forcedGcCount:%lu "
						"queueLenCall:%u queueLenReply:%u\n", _pipelineId,
						_goodPduCount, _badPduCount, 
						_completePduCallCount, _completePduReplyCount, 
						_completeHdrPduCallCount, _completeHdrPduReplyCount,
						_unmatchedCallCount, _unmatchedReplyCount, 
						_scannedRpcCallHdrs, _scannedRpcReplyHdrs, 
						_forcedRpcReplyScanCount, _forcedGcCount,
						rpcReverseFlowDesc->queueLen, rpcFlowDesc->queueLen);
					_goodPduPrintLimit += 100000;
				}
				#endif
				rpcReverseFlowDesc->passReadyPdus(_sink, this);
			}
			if (startPktDesc == NULL) // examined all the packets
				break;
		}
	} while ((pduDesc != NULL && rpcFlowDesc->head != NULL));
	if (pduDesc == NULL && rpcFlowDesc->head != NULL &&
			rpcFlowDesc->head->visitCount > RPC_PARSER_SCAN_THRESH) {
		// scan for RPC header
		if (!scanForRpcHeader(rpcFlowDesc, &startPktDesc)) {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
			printf("unsuccessful scan!\n");
			#endif
			if (gcPktsByVisitCount(rpcFlowDesc, rpcFlowDesc->head,
					RPC_PARSER_GC_THRESH)) {
				#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC)
				printf("GC: after unsuccessful scan\n");
				#endif
			} 
		} else {
			#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
			printf("successful scan!\n");
			#endif
			// more efficient to do the processing later
		} 
	}
}

PduDescriptor *
RpcParser::findPdu(FlowDescriptorTcpRpc *flowDesc, 
	CompletePduAction &action, PacketDescriptor **startPktDesc)
{
	uint32_t pduLen, xid, prog, progVersion, progProc, msgType, version, jump,
		acceptState = 0;
	PduDescriptor *pduDesc;
	PacketDescriptor *pktDesc, *firstPktDesc;
	uint16_t index, rpcHdrIndex;

	if (*startPktDesc == NULL)
		pktDesc = flowDesc->head;
	else
		pktDesc = *startPktDesc;
	
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	if (pktDesc != NULL)
		printf("**************************\n");
	#endif

	while (pktDesc != NULL) {
		if (pktDesc->toParseOffset > pktDesc->pcapHeader.caplen) {
			printf("pktDesc->toParseOffset:%u pktDesc->pcapHeader.caplen:%u "
				"pktDesc->pcapHeader.len:%u\n",
				pktDesc->toParseOffset, pktDesc->pcapHeader.caplen,
				pktDesc->pcapHeader.len);
			pktDesc->print();
			assert(false);
		}
		index = pktDesc->toParseOffset;
		_streamNavigator->init(pktDesc, index);
		#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
		printf("pkt:%u visit:%u toParseOffset:%u\n", pktDesc->packetBufferIndex, 
			pktDesc->visitCount, index);
		#endif
		if (pktDesc->destPort == NFS_PORT 
				|| pktDesc->destPort == SUNRPC_PORT)	{ // RPC call
			if (pktDesc->visitCount > RPC_PARSER_GC_THRESH) {
			// successful scan but RPC header was not matched 
				if (gcPktsByVisitCount(flowDesc, pktDesc, 
						RPC_PARSER_GC_THRESH + 1)) {
					#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC)		
					printf("GC: CALL false-positive scan/garbage\n");
					#endif
					pktDesc = flowDesc->head;
					if (pktDesc == NULL)
						return NULL;
					continue;
				}
			}
			// to maintain sequentiality and not to punish packets with multiple PDUs
			if (*startPktDesc == flowDesc->head || pktDesc != *startPktDesc)
				pktDesc->visitCount++;
			/* TODO: DEL (not valid for unaligned PDUs)
			if ((pktDesc->payloadOffset + pktDesc->payloadLength - index
					>= MIN_RPC_CALL_HEADER_LEN) ||
					index != pktDesc->payloadOffset) { // large enough packet
			}
			*/
			firstPktDesc = _streamNavigator->getPacketDesc();
			rpcHdrIndex = _streamNavigator->getIndex();
			if (!_streamNavigator->getUint32Packet(&pduLen)) 
				goto next;
			if (!(pduLen & 0x80000000))  
				goto next;
			pduLen = pduLen & 0x7fffffff;
			if (!_streamNavigator->getUint32Packet(&xid))
				goto next;
			if (!_streamNavigator->getUint32Packet(&msgType))
				goto next;
			if (!_streamNavigator->getUint32Packet(&version))
				goto next;
			if (msgType == RPC_CALL && version == RPC_VERSION) {
				if (!_streamNavigator->getUint32Packet(&prog))
					goto next;
				if (!_streamNavigator->getUint32Packet(&progVersion))
					goto next;
				if (!_streamNavigator->getUint32Packet(&progProc))
					goto next;
				if (!_streamNavigator->skipBytesPacket(4))
					goto next;
				if (!_streamNavigator->getUint32Packet(&jump))
					goto next;
				if (!_streamNavigator->skipBytesPacket(jump + 4))
					goto next;
				if (!_streamNavigator->getUint32Packet(&jump))
					goto next;
				if (!_streamNavigator->skipBytesPacket(jump))
					goto next;
				switch(prog) {
					case NFS_PROGRAM:
						if ((pduDesc = findNfsPdu(flowDesc, firstPktDesc,
								_streamNavigator->getPacketDesc(),
								rpcHdrIndex, _streamNavigator->getIndex(), 
								pduLen, xid, progVersion,
								progProc, acceptState, RPC_CALL)) 
								!= NULL) {
							if (pduDesc->lastPktDesc->toParseOffset 
									< pduDesc->lastPktDesc->pcapHeader.caplen) {
								*startPktDesc = pduDesc->lastPktDesc;
							}
							else
								*startPktDesc = pduDesc->lastPktDesc->next;
							action = ACTION_INSERT_CALL;
							// ignoring duplicate xids due to retransmission
							if (flowDesc->hasSeenCallPdu(pduDesc->rpcXid)) {
								PacketDescriptor *tmpPktDesc;
								#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
								if (pduDesc->rpcPduType ==
										PduDescriptor::PDU_COMPLETE)
									_completePduCallCount--;
								else if (pduDesc->rpcPduType ==
										PduDescriptor::PDU_COMPLETE_HEADER)
									_completeHdrPduCallCount--;
								#endif
								pduDesc->rpcPduType = PduDescriptor::PDU_BAD;
								pduDesc->rpcProgram = 
									pduDesc->rpcProgramVersion = 
									pduDesc->rpcProgramProcedure =	RPC_BAD_PDU;
								// cleanup	
								*startPktDesc = NULL;
								action = ACTION_NONE;
								tmpPktDesc = _streamNavigator->getPacketDesc();
								if (tmpPktDesc->toParseOffset == 
										tmpPktDesc->pcapHeader.caplen &&
										tmpPktDesc->next != NULL) {
									_streamNavigator->init(tmpPktDesc->next, 
										tmpPktDesc->next->toParseOffset);
								}
								passBadPdu(pduDesc);
								goto next;
							}							
							return pduDesc;
						}
						break;
					case NFS_MNT_PROGRAM:
						// TODO: handling mount
						break;
					case RPC_PORTMAP_PROGRAM:
						// TODO: handling portmap
						break;
				}
			}
		} else { // RPC reply
			PduDescriptor *callPduDesc;
			FlowDescriptorTcpRpc *reverseFlowDesc = 
				static_cast<FlowDescriptorTcpRpc *> 
				(flowDesc->flowDescriptorReverse);

			// check if reply is being processed before the call
			if (reverseFlowDesc->head != NULL &&
					pktDesc->tcpAckNum > reverseFlowDesc->head->tcpSeqNum) {
				if (pktDesc->tcpAckNum - reverseFlowDesc->head->tcpSeqNum
						<= MAX_TCP_RECV_WINDOW) { // not a tcp seq wrap-around
					//BEGIN
					if (pktDesc->tcpAckNum > reverseFlowDesc->maxSeqNumSeen
							&& pktDesc->tcpAckNum - reverseFlowDesc->maxSeqNumSeen <= MAX_TCP_RECV_WINDOW) {
						// replies are ahead of calls; either delay processing
						// or force garbage collection
						if (flowDesc->queueLen > 
								(RPC_PARSER_REPLY_PARSE_THRESH << 1)) {
							// we're gettign ahead of ourselves!
							// capping the reply packets by garbage 
							// collecting them since replies are way
							// ahead of calls
							pduDesc = constructBadPdu(flowDesc->head, 
							//pduDesc = constructBadPdu(pktDesc,
								flowDesc->tail);
							if (pduDesc != NULL) {
								#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
								_forcedGcCount++;
								#endif
								flowDesc->releasePktDescriptors(
								pduDesc->lastPktDesc, true);
								passBadPdu(pduDesc);
							}
						}
						return NULL;
					} else {
						if (flowDesc->queueLen < 
								RPC_PARSER_REPLY_PARSE_THRESH) {
							// delay processing this packet
							#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
							printf("delay pktDesc->tcpAckNum: %u " 
								"reverseFlowDesc->head->tcpSeqNum: %u\n",
								pktDesc->tcpAckNum, reverseFlowDesc->head->tcpSeqNum);
							#endif
							return NULL;
						} else {
							#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
							_forcedRpcReplyScanCount++;
							#endif
							if (flowDesc->head->visitCount 
								< RPC_PARSER_COMPLETE_HDR_THRESH)
							flowDesc->increaseVisitCount(
								uint32_t 
								(flowDesc->queueLen - 
									RPC_PARSER_GC_THRESH),
								RPC_PARSER_COMPLETE_HDR_THRESH);					
						}
					}
				} //END
				/* //OLD_BEGIN
					if (reverseFlowDesc->head->pcapHeader.ts.tv_sec - 
							pktDesc->pcapHeader.ts.tv_sec < 
							RPC_PARSER_CALL_REPLY_SYNC_WAIT_TIME) {
						// packet and head of reverse are within the sync
						// wait time (so no stale head is holding us back).
						// now we need to prevent out of order call packets
						// near head on the reverse flow from holding us 
						// back. this is to ensure progress and bounded 
						// wait time/memory utilization.
						if (flowDesc->queueLen < 
								RPC_PARSER_REPLY_PARSE_THRESH) { 
							// delay processing this packet
							#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
							printf("delay pktDesc->tcpAckNum: %u " 
								"reverseFlowDesc->head->tcpSeqNum: %u\n",
								pktDesc->tcpAckNum, reverseFlowDesc->head->tcpSeqNum);
							#endif
							return NULL;
						} else {
							if (flowDesc->queueLen > 
									(RPC_PARSER_REPLY_PARSE_THRESH << 1)) {
								// capping the reply packets by garbage 
								// collecting them all
								pduDesc = constructBadPdu(flowDesc->head, 
									flowDesc->tail);
								if (pduDesc != NULL) {
									#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
									_forcedGcCount++;
									#endif
									flowDesc->releasePktDescriptors(
									pduDesc->lastPktDesc, true);
									passBadPdu(pduDesc);
									return NULL;
								}
							}
							#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
							_forcedRpcReplyScanCount++;
							#endif
							// making sure we are not stuck because call
							// packets have n't reached the parsing 
							// threshold.
							//if (reverseFlowDesc->queueLen < )
						}
						if (flowDesc->head->visitCount 
								< RPC_PARSER_COMPLETE_HDR_THRESH)
							flowDesc->increaseVisitCount(
								uint32_t 
								(3*RPC_PARSER_REPLY_PARSE_THRESH/4),
								RPC_PARSER_COMPLETE_HDR_THRESH);
					}
				} //OLD_END */
			} else if (pktDesc->tcpAckNum > reverseFlowDesc->maxSeqNumSeen) {
				if (reverseFlowDesc->maxSeqNumSeen == 0 ||
						(pktDesc->tcpAckNum - reverseFlowDesc->maxSeqNumSeen
						<= MAX_TCP_RECV_WINDOW)) { // not a tcp seq wrap-around
					#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
					printf("delay pktDesc->tcpAckNum: %u "
						"reverseFlowDesc-maxSeqNumSeen: %u\n",
						pktDesc->tcpAckNum, reverseFlowDesc->maxSeqNumSeen);
					#endif
					if (flowDesc->queueLen > 
							(RPC_PARSER_REPLY_PARSE_THRESH << 1)) {
						// capping the reply packets by garbage collecting them all
						pduDesc = constructBadPdu(flowDesc->head, flowDesc->tail);
						if (pduDesc != NULL) {
							#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
							_forcedGcCount++;
							#endif
							flowDesc->releasePktDescriptors(
								pduDesc->lastPktDesc, true);
							passBadPdu(pduDesc);
						}
					}
					return NULL; // delay parsing this packet
				} 
			}

			if (pktDesc->visitCount > RPC_PARSER_GC_THRESH) {
			// false-positive scan or incomplete header gets us here
			// +1 so we can find unmatched replies first (see below)
				if (gcPktsByVisitCount(flowDesc, pktDesc, 
						RPC_PARSER_GC_THRESH + 1)) {
					#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC)		
					printf("GC: REPLY false-positive scan/garbage (see above)\n");
					#endif
					pktDesc = flowDesc->head;
					if (pktDesc == NULL)
						return NULL;
					continue;
				}
			}
			// to maintain sequentiality and not to punish packets with multiple PDUs
			if (*startPktDesc == flowDesc->head || pktDesc != *startPktDesc)
				pktDesc->visitCount++;
			/* TODO: DEL (not valid for unaligned PDUs)
			if ((pktDesc->payloadOffset + pktDesc->payloadLength - index
					>= MIN_RPC_REPLY_HEADER_LEN) ||
					index != pktDesc->payloadOffset) { // large enough packet
			}
			*/
			firstPktDesc = _streamNavigator->getPacketDesc();
			rpcHdrIndex = _streamNavigator->getIndex();
			if (!_streamNavigator->getUint32Packet(&pduLen))
				goto next;
			if (!(pduLen & 0x80000000))  
				goto next;
			pduLen = pduLen & 0x7fffffff;
			if (!_streamNavigator->getUint32Packet(&xid))
				goto next;
			if (!_streamNavigator->getUint32Packet(&msgType) || msgType != RPC_REPLY)
				goto next;
			if ((callPduDesc = reverseFlowDesc->getCallPdu(xid))
					== NULL) {
				PacketDescriptor *tmpPktDesc = _streamNavigator->getPacketDesc();
				if (tmpPktDesc && 
						tmpPktDesc->visitCount >= RPC_PARSER_GC_THRESH) {
					#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC)  				
					printf("GC: unmatched reply for xid:%#010x\n", xid);
					#endif
					#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
					// not quite accurate due to possible recounting
					_unmatchedReplyCount++; 
					#endif
					gcPktsUpto(flowDesc, tmpPktDesc);
					pktDesc = flowDesc->head;
					if (pktDesc == NULL)
						return NULL;
					continue;
				}
				goto next;
			}
			if (!_streamNavigator->skipBytesPacket(4)) {
				reverseFlowDesc->insertCallPdu(callPduDesc);
				goto next;
			}
			if (!_streamNavigator->getUint32Packet(&jump)) {
				reverseFlowDesc->insertCallPdu(callPduDesc);
				goto next;	
			}
			if (!_streamNavigator->skipBytesPacket(jump + 4)) {
				reverseFlowDesc->insertCallPdu(callPduDesc);
				goto next;
			}
			if (!_streamNavigator->getUint32Packet(&acceptState)) {
				reverseFlowDesc->insertCallPdu(callPduDesc);
				goto next;
			}

			switch(callPduDesc->rpcProgram) {
				case NFS_PROGRAM:
					if ((pduDesc = findNfsPdu(flowDesc, firstPktDesc,
							_streamNavigator->getPacketDesc(), 
							rpcHdrIndex, _streamNavigator->getIndex(),
							pduLen, xid, callPduDesc->rpcProgramVersion, 
							callPduDesc->rpcProgramProcedure,
							acceptState, RPC_REPLY)) != NULL) {
						callPduDesc->next = pduDesc;
						if (pduDesc->lastPktDesc->toParseOffset 
								< pduDesc->lastPktDesc->pcapHeader.caplen)
							*startPktDesc = pduDesc->lastPktDesc;
						else
							*startPktDesc = pduDesc->lastPktDesc->next;
						
						action = ACTION_PASS_REPLY;
						return callPduDesc;
					}
					break;
				case NFS_MNT_PROGRAM:
					// TODO: handling mount
					break;
				case RPC_PORTMAP_PROGRAM:
					// TODO: handling portmap
					break;
			}
			// if no matching reply pdu, reinsert the call pdu
			reverseFlowDesc->insertCallPdu(callPduDesc);
		}
	next:
		// have n't found a PDU, check the next packet
		if (pktDesc != _streamNavigator->getPacketDesc())
			pktDesc = _streamNavigator->getPacketDesc();
		else
			pktDesc = pktDesc->next;
	}
	return NULL;
}

PduDescriptor *
RpcParser::findNfsPdu(FlowDescriptorTcpRpc *flowDesc, 
	PacketDescriptor *firstPktDesc,	PacketDescriptor *firstProgPktDesc,
	uint16_t rpcHeaderOffset, uint16_t rpcProgOffset,
	uint32_t pduLen, uint32_t xid, uint32_t progVersion, uint32_t progProc,
	uint16_t acceptState, uint8_t msgType)
{
	PduDescriptor *pduDesc, *retPduDesc;
	// handling false-positive RPC headers early
	if(firstPktDesc != firstProgPktDesc && firstPktDesc->next != firstProgPktDesc)
		return NULL;
	if (progVersion == NFS3_VERSION && progProc <= NFS3PROC_COMMIT) { 
		pduDesc = new NfsV3PduDescriptor(firstPktDesc, firstProgPktDesc, 
			pduLen, xid, progVersion, progProc, rpcHeaderOffset, rpcProgOffset,
			acceptState, msgType);
		if ((retPduDesc = constructAndDetachPdu(flowDesc, pduDesc)) 
				!= NULL) 
			return retPduDesc;			
		else {
			delete pduDesc;
			return NULL;
		}
	} else {
		//TODO: handling other NFS versions
		return NULL;
	}
}

PduDescriptor *
RpcParser::constructAndDetachPdu(FlowDescriptorTcpRpc *flowDesc, 
	PduDescriptor *pduDesc)
{
	if (constructPdu(flowDesc, pduDesc)) { // success!
		if (pduDesc->firstPktDesc != flowDesc->head &&
				(flowDesc->head->flag & PACKET_SCANNED)) {
			/* // TODO: DEL
			uint16_t before = flowDesc->queueLen;
			printf("\nreduced queue len from %u ", flowDesc->queueLen); */
			if (gcPktsUpto(flowDesc, pduDesc->firstPktDesc->prev)) {
				#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC) 
				printf("GC: after finding a valid Pdu\n");
				#endif
			}
			/* // TODO: DEL 
			printf("to %u = %u pduDesc->firstPktDesc->payloadOffset:%u "
				"pduDesc->firstPktDesc->rpcHeaderOffset:%u\n", 
				flowDesc->queueLen, before - flowDesc->queueLen, 
				pduDesc->firstPktDesc->payloadOffset, pduDesc->rpcHeaderOffset); */
		}
		if (pduDesc->lastPktDesc->toParseOffset 
				< pduDesc->lastPktDesc->pcapHeader.caplen) { 
			// potentially more pdus in the last packet of the pdu
			flowDesc->releasePktDescriptors(pduDesc->firstPktDesc, 
				pduDesc->lastPktDesc, false);
			pduDesc->lastPktDesc->refCount++;
		} else
			flowDesc->releasePktDescriptors(pduDesc->firstPktDesc,
				pduDesc->lastPktDesc, true);		
		#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
		pduDesc->print();
		printf("lastPktDesc->toParseoffset:%u lastPktDesc->packetLen:%u\n", 
			pduDesc->lastPktDesc->toParseOffset, 
			pduDesc->lastPktDesc->pcapHeader.caplen);
		printf("queueLen: %u\n", flowDesc->queueLen);
		#endif
		return pduDesc;
	} else // not enough packets to construct this pdu; wait till later!
		return NULL;
}

bool
RpcParser::constructPdu(FlowDescriptorTcpRpc *flowDesc, PduDescriptor *pduDesc)
{
	uint32_t pduLen, remainedPduLen, maxSeqNumNeeded;
	PacketDescriptor *pktDesc;
	pduLen = pduDesc->rpcPduLen;
	remainedPduLen = pduLen - _streamNavigator->getParsedBytes() + 4;
	
	// making sure enough packets are there before we go through the list
	pktDesc = _streamNavigator->getPacketDesc();
	maxSeqNumNeeded = pktDesc->tcpSeqNum + (_streamNavigator->getIndex() - 
		pktDesc->payloadOffset) + remainedPduLen;
	if (maxSeqNumNeeded > flowDesc->maxSeqNumSeen) {
		if (maxSeqNumNeeded - flowDesc->maxSeqNumSeen < MAX_TCP_RECV_WINDOW) {
			_streamNavigator->init(flowDesc->tail, 0);
			return false;
		}
	}
	
	if (!_streamNavigator->skipBytesPacket(remainedPduLen)) {
		if (pduDesc->firstPktDesc->visitCount >= 
				RPC_PARSER_COMPLETE_HDR_THRESH) {
			pduDesc->rpcPduType = PduDescriptor::PDU_COMPLETE_HEADER;
			#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
			if (flowDesc->sourcePort == NFS_PORT)
				_completeHdrPduReplyCount++;
			else
				_completeHdrPduCallCount++;
			#endif	
			pktDesc = _streamNavigator->getPacketDesc();
			if (pktDesc == NULL)
				pduDesc->lastPktDesc = flowDesc->tail;
			else { // COMPLETE_HEADER PDU followed by a hole 
				if (pktDesc == pduDesc->firstPktDesc)
					pduDesc->lastPktDesc = pktDesc;
				else
					pduDesc->lastPktDesc = pktDesc->prev;
			}
			assert(pduDesc->lastPktDesc != NULL);
			pduDesc->rpcProgramEndOffset = 
				pduDesc->lastPktDesc->pcapHeader.caplen	- 1;
			pduDesc->lastPktDesc->toParseOffset = 
				pduDesc->lastPktDesc->pcapHeader.caplen;
			return true;
		}
		return false;
	}
	pduDesc->rpcPduType = PduDescriptor::PDU_COMPLETE;
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	if (flowDesc->sourcePort == NFS_PORT)
		_completePduReplyCount++;
	else
		_completePduCallCount++;
	#endif	
	pduDesc->lastPktDesc = _streamNavigator->getPacketDesc();
	assert(pduDesc->lastPktDesc != NULL);
	pduDesc->lastPktDesc->toParseOffset = _streamNavigator->getIndex();
	pduDesc->rpcProgramEndOffset = pduDesc->lastPktDesc->toParseOffset - 1;
	return true;
}

PduDescriptor *
RpcParser::constructBadPdu(PacketDescriptor *firstPktDesc, 
	PacketDescriptor *lastPktDesc)
{
	PduDescriptor *badPduDesc;
	badPduDesc = new BadPduDescriptor(firstPktDesc, lastPktDesc);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	badPduDesc->print();
	#endif
	return badPduDesc;
}

PduDescriptor *
RpcParser::constructBadPduBySize(FlowDescriptorTcpRpc *flowDesc, int size)
{
	PduDescriptor *badPduDesc;
	PacketDescriptor *firstPktDesc, *pktDesc;
	if (flowDesc->queueLen < size)
		return NULL;
	firstPktDesc = flowDesc->head;
	int i = 0;
	for (pktDesc = firstPktDesc; i < size - 1; i++) 
		pktDesc = pktDesc->next;
	badPduDesc = new BadPduDescriptor(firstPktDesc, pktDesc);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	badPduDesc->print();
	#endif
	return badPduDesc;
}

PduDescriptor *
RpcParser::constructBadPduByVisit(FlowDescriptorTcpRpc *flowDesc, 
	PacketDescriptor *pktDesc, uint16_t visitCount)
{
	PduDescriptor *badPduDesc;
	if (pktDesc->visitCount <= visitCount)
		return NULL;
	while (pktDesc != NULL && pktDesc->visitCount >= visitCount) 
		pktDesc = pktDesc->next;
	if (pktDesc == NULL)
		pktDesc = flowDesc->tail;
	else
		pktDesc = pktDesc->prev;
	badPduDesc = new BadPduDescriptor(flowDesc->head, pktDesc);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	badPduDesc->print();
	#endif
	return badPduDesc;
}

PduDescriptor *
RpcParser::constructBadPduByTime(FlowDescriptorTcpRpc *flowDesc, 
	time_t ts)
{
	PduDescriptor *badPduDesc;
	PacketDescriptor *pktDesc = flowDesc->head;
	if (pktDesc->pcapHeader.ts.tv_sec > ts)
		return NULL;
	while (pktDesc != NULL && pktDesc->pcapHeader.ts.tv_sec <= ts
			&& pktDesc->visitCount > 0)	
		pktDesc = pktDesc->next;
	if (pktDesc == NULL)
		pktDesc = flowDesc->tail;
	else
		pktDesc = pktDesc->prev;
	if (pktDesc == NULL)
		pktDesc = flowDesc->head;
	badPduDesc = new BadPduDescriptor(flowDesc->head, pktDesc);
	#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC)
	badPduDesc->print();
	#endif
	return badPduDesc;
}

void
RpcParser::passBadPdu(PduDescriptor *pduDesc)
{
	#if CHRON_DEBUG(CHRONICLE_DEBUG_BUFPOOL)
	pduDesc->tagPduPkts(PROCESS_RPC_PARSER);
	#endif
	#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
	_badPduCount++;
	if (_badPduCount >= _badPduPrintLimit) {
		printf("RpcParser::passBadPdu: goodPduCount:%lu badPduCount:%lu\n",
			_goodPduCount, _badPduCount);
		_badPduPrintLimit += 100000;
	}
	#endif
	_sink->processRequest(this, pduDesc);
}

bool
RpcParser::scanForRpcHeader(FlowDescriptorTcpRpc *flowDesc, 
	PacketDescriptor **rpcHeadPktDesc)
{
	PacketDescriptor *pktDesc = flowDesc->firstUnscannedPkt;
	if (pktDesc == NULL)
		return false;
	uint16_t index = pktDesc->toParseOffset;
	bool found = false, call = false;
	uint32_t bytes;
	uint32_t signature[3];

	FlowDescriptorTcpRpc *flowDescReverse = 
		static_cast<FlowDescriptorTcpRpc *> (flowDesc->flowDescriptorReverse);

	if (pktDesc->destPort == NFS_PORT 
			|| pktDesc->destPort == SUNRPC_PORT) // RPC call
		call = true;
	_streamNavigator->init(pktDesc, index);
	while (!found && (pktDesc = _streamNavigator->getPacketDesc()) != NULL) {
		if (pktDesc->flag & PACKET_SCANNED) {
			pktDesc = flowDesc->findNextUnscannedPkt(pktDesc);
			if (flowDesc->firstUnscannedPkt->flag & PACKET_SCANNED)
				flowDesc->firstUnscannedPkt = pktDesc;
			if (pktDesc == NULL)
				return false;
			index = pktDesc->toParseOffset;
			_streamNavigator->init(pktDesc, index);
		} else {
			if (flowDesc->firstUnscannedPkt->flag & PACKET_SCANNED)
				flowDesc->firstUnscannedPkt = _streamNavigator->getPacketDesc();
		}
		if (call) {	// RPC call pattern
			if (!_streamNavigator->getUint32Packet(&signature[0], true))
				goto ooo_packet;
			switch(signature[0]) {
				case 0x0:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					switch(signature[1]) {
						case 0x00000200:
							if (!_streamNavigator->getUint32Packet(&signature[2], 
									true))
								goto ooo_packet;
							if (signature[2] == 0x0186a300) {
								found = true;
								bytes = 21;	
							} else if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
							break;
						case 0x00020001:
							if (!_streamNavigator->getUint32Packet(&signature[2], 
									true))
								goto ooo_packet;
							if (signature[2] == 0x86a30000) {
								found = true;
								bytes = 22;	
							} else if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
							break;
						case 0x02000186:
							if (!_streamNavigator->getUint32Packet(&signature[2], 
									true))
								goto ooo_packet;
							if (signature[2] == 0xa3000000) {
								found = true;
								bytes = 23;	
							} else if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);	
							break;
					}
					if (!found && MATCH_SIGN0_CALL(signature[1])) {
						_streamNavigator->goBackBytesPacket(4, true);
						continue;
					}
					break;
				case 0x00000002:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0x000186a3) {
						if (!_streamNavigator->getUint32Packet(&signature[2], true))
							goto ooo_packet;
						if (signature[2] == 0x00000003) {
							found = true;
							bytes = 24;	
						} else {
							if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
						}
					}
					if (!found && MATCH_SIGN0_CALL(signature[1])) {
						_streamNavigator->goBackBytesPacket(4, true);
						continue;
					}
					break;
				case 0x00000200:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0x0186a300) {
						if (!_streamNavigator->getUint32Packet(&signature[2], true))
							goto ooo_packet;
						if (signature[2] == 0x00000300) {
							found = true;
							bytes = 25;	
						} else {
							if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
						}
					}
					if (!found && MATCH_SIGN0_CALL(signature[1])) {
						_streamNavigator->goBackBytesPacket(4, true);
						continue;
					}
					break;
				case 0x00020001:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0x86a30000) {
						if (!_streamNavigator->getUint32Packet(&signature[2], true))
							goto ooo_packet;
						if (signature[2] == 0x00030000) {
							found = true;
							bytes = 26;	
						} else {
							if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
						}
					}
					if (!found && MATCH_SIGN0_CALL(signature[1])) {
						_streamNavigator->goBackBytesPacket(4, true);
						continue;
					}
					break;
				case 0x02000186:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0xa3000000) {
						if (!_streamNavigator->getUint32Packet(&signature[2], true))
							goto ooo_packet;
						if (signature[2] == 0x03000000) {
							found = true;
							bytes = 27;	
						} else {
							if (MATCH_SIGN0_CALL(signature[2]) ||
									MATCH_SIGN1_CALL(signature[2]))
								_streamNavigator->goBackBytesPacket(4, true);
						}
					}
					if (!found && MATCH_SIGN0_CALL(signature[1])) {
						_streamNavigator->goBackBytesPacket(4, true);
						continue;
					}
					break;
			}
		} else {	// RPC reply pattern
			if (!_streamNavigator->getUint32Packet(&signature[0], true))
				goto ooo_packet;
			switch(signature[0]) {
				case 0x0:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					switch(signature[1]) {
						case 0x0:							
						case 0x01:
						case 0x02:
						case 0x03:
						case 0x04:
							index = _streamNavigator->getIndex();
							pktDesc = _streamNavigator->getPacketDesc();
							if (!_streamNavigator->goBackBytesPacket(16, true)) {
								_streamNavigator->init(pktDesc, index);
							} else {
								uint32_t xid;
								if (!_streamNavigator->getUint32Packet(&xid, true))
									assert(false);
								else if (flowDescReverse->hasSeenCallPdu(xid) &&
										_streamNavigator->goBackBytesPacket(8, true)) {
									found = true;
									bytes = 0;
								}
								if (!found) {
									if (MATCH_SIGN0_REPLY(signature[1])) {
										_streamNavigator->init(pktDesc, index);
										_streamNavigator->goBackBytesPacket(4, true);
									} else
										_streamNavigator->init(pktDesc, index);
								}
							}
							break;
						default:
							if (MATCH_SIGN0_REPLY(signature[1]))
								_streamNavigator->goBackBytesPacket(4, true);
					}
					break;
				case 0x01000000:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0) {
						index = _streamNavigator->getIndex();
						pktDesc = _streamNavigator->getPacketDesc();
						if (!_streamNavigator->goBackBytesPacket(15, true)) {
							_streamNavigator->init(pktDesc, index);
						} else {
							uint32_t xid;
							if (!_streamNavigator->getUint32Packet(&xid, true))
								assert(false);
							else if (flowDescReverse->hasSeenCallPdu(xid) &&
									_streamNavigator->goBackBytesPacket(8, true)) {
								found = true;
								bytes = 0;
							} 
							if (!found) {
								if (MATCH_SIGN0_REPLY(signature[1])) {
									_streamNavigator->init(pktDesc, index);
									_streamNavigator->goBackBytesPacket(4, true);
								} else
									_streamNavigator->init(pktDesc, index);
							}
						}
					} else {
						if (MATCH_SIGN0_REPLY(signature[1]))
							_streamNavigator->goBackBytesPacket(4, true);
					}
					break;
				case 0x00010000:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0) {
						index = _streamNavigator->getIndex();
						pktDesc = _streamNavigator->getPacketDesc();
						if (!_streamNavigator->goBackBytesPacket(14, true)) {
							_streamNavigator->init(pktDesc, index);
						} else {
							uint32_t xid;
							if (!_streamNavigator->getUint32Packet(&xid, true))
								assert(false);
							else if (flowDescReverse->hasSeenCallPdu(xid) &&
									_streamNavigator->goBackBytesPacket(8, true)) {
								found = true;
								bytes = 0;
							} 
							if (!found) {
								if (MATCH_SIGN0_REPLY(signature[1])) {
									_streamNavigator->init(pktDesc, index);
									_streamNavigator->goBackBytesPacket(4, true);
								} else
									_streamNavigator->init(pktDesc, index);
							}
						}
					} else {
						if (MATCH_SIGN0_REPLY(signature[1]))
							_streamNavigator->goBackBytesPacket(4, true);
					}
					break;
				case 0x00000100:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0) {
						index = _streamNavigator->getIndex();
						pktDesc = _streamNavigator->getPacketDesc();
						if (!_streamNavigator->goBackBytesPacket(13, true)) {
							_streamNavigator->init(pktDesc, index);
						} else {
							uint32_t xid;
							if (!_streamNavigator->getUint32Packet(&xid, true))
								assert(false);
							else if (flowDescReverse->hasSeenCallPdu(xid) &&
									_streamNavigator->goBackBytesPacket(8, true)) {
								found = true;
								bytes = 0;
							} 
							if (!found) {
								if (MATCH_SIGN0_REPLY(signature[1])) {
									_streamNavigator->init(pktDesc, index);
									_streamNavigator->goBackBytesPacket(4, true);
								} else
									_streamNavigator->init(pktDesc, index);
							}
						}
					} else {
						if (MATCH_SIGN0_REPLY(signature[1]))
							_streamNavigator->goBackBytesPacket(4, true);
					}
					break;
				case 0x00000001:
					if (!_streamNavigator->getUint32Packet(&signature[1], true))
						goto ooo_packet;
					if (signature[1] == 0) {
						index = _streamNavigator->getIndex();
						pktDesc = _streamNavigator->getPacketDesc();
						if (!_streamNavigator->goBackBytesPacket(12, true)) {
							_streamNavigator->init(pktDesc, index);
						} else {
							uint32_t xid;
							if (!_streamNavigator->getUint32Packet(&xid, true))
								assert(false);
							else if (flowDescReverse->hasSeenCallPdu(xid) &&
									_streamNavigator->goBackBytesPacket(8, true)) {
								found = true;
								bytes = 0;
							} 
							if (!found) {
								if (MATCH_SIGN0_REPLY(signature[1])) {
									_streamNavigator->init(pktDesc, index);
									_streamNavigator->goBackBytesPacket(4, true);
								} else
									_streamNavigator->init(pktDesc, index);
							}
						}
					} else {
						if (MATCH_SIGN0_REPLY(signature[1]))
							_streamNavigator->goBackBytesPacket(4, true);
					}
					break;
				default:
					;
			}
		}
		if (found) {
			index = _streamNavigator->getIndex();
			pktDesc = _streamNavigator->getPacketDesc();
			if (!_streamNavigator->goBackBytesPacket(bytes, true)) {
				found = false;
				_streamNavigator->init(pktDesc, index);
			}
			else {
				*rpcHeadPktDesc = _streamNavigator->getPacketDesc();
				(*rpcHeadPktDesc)->flag |= PACKET_SCANNED;
				(*rpcHeadPktDesc)->toParseOffset = _streamNavigator->getIndex();

				if (*rpcHeadPktDesc != flowDesc->head) {
					#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
					if (call)
						_scannedRpcCallHdrs++;
					else
						_scannedRpcReplyHdrs++;
					#endif
					// only when retransmissions are unlikely
					// (i.e., forced reply parsing scenario)
					/* // commented out as it significantly reduces the number
					   // of parsable reply PDUs.
					if (!call 
							&& flowDesc->queueLen > (3*RPC_PARSER_REPLY_PARSE_THRESH/4)) {
						#if CHRON_DEBUG(CHRONICLE_DEBUG_RPC | CHRONICLE_DEBUG_GC)
						printf("GC: after successful scan ");
						if (call)
							printf("CALL ");
						else
							printf("REPLY ");
						printf("(flowHead->index:%u rpcHead->index:%u)\n", 
							(*rpcHeadPktDesc)->packetBufferIndex, 
							flowDesc->head->packetBufferIndex);	
						#endif	   
						PduDescriptor *badPduDesc = 
							constructBadPdu(flowDesc->head, 
								(*rpcHeadPktDesc)->prev);
						flowDesc->releasePktDescriptors(
							badPduDesc->lastPktDesc, true);
						passBadPdu(badPduDesc);
					}*/
				}
				if (flowDesc->firstUnscannedPkt != NULL && 
						(flowDesc->firstUnscannedPkt->flag & PACKET_SCANNED))
					flowDesc->firstUnscannedPkt = 
						flowDesc->findNextUnscannedPkt(
						flowDesc->firstUnscannedPkt);
				return true;
			}
		}
		continue;
	ooo_packet:
		pktDesc = _streamNavigator->getPacketDesc(); 
		// To avoid rescanning the same packet, we flag a packet as scanned if
		// the previous and subsequent packets are not NULL and is in order 
		// (making an exception for head; see TcpStreamNavigator::advanceToNextPacket).
		if (pktDesc != NULL) { // out of order packet
			index = pktDesc->toParseOffset;
			_streamNavigator->init(pktDesc, index);			
		}
	}
	return false;
}

bool
RpcParser::gcPktsByVisitCount(FlowDescriptorTcpRpc *flowDesc, 
	PacketDescriptor *pktDesc, uint16_t visitCount)
{
	PduDescriptor *badPduDesc;
	badPduDesc = constructBadPduByVisit(flowDesc, pktDesc, visitCount);
	if (badPduDesc != NULL) {
		/* //TODO: DEL
		uint16_t queueLen = flowDesc->queueLen; */
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
		printf("===============\n");
		printf("GC1: badPduDesc->firstPktDesc->visitCount:%u before:%u", 
			badPduDesc->firstPktDesc->visitCount, flowDesc->queueLen);
		#endif
		flowDesc->releasePktDescriptors(badPduDesc->firstPktDesc, 
			badPduDesc->lastPktDesc, true);
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
		printf(" after:%u tcp_seq:%u\n", flowDesc->queueLen, 
			badPduDesc->firstPktDesc->tcpSeqNum);
		badPduDesc->print();
		#endif
		/* //TODO: DEL
		if (queueLen - flowDesc->queueLen > 1000)
			printf("\nRpcParser::gcPktsByVisitCount: before:%u after:%u\n",
				queueLen, flowDesc->queueLen); */
		passBadPdu(badPduDesc);
		return true;
	}
	return false;
}

bool
RpcParser::gcPktsUpto(FlowDescriptorTcpRpc *flowDesc, 
	PacketDescriptor *pktDesc)
{
	PduDescriptor *badPduDesc;
	badPduDesc = constructBadPdu(flowDesc->head, pktDesc);
	if (badPduDesc != NULL) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC) 
		printf("===============\n");
		printf("GC2: badPduDesc->firstPktDesc->visitCount:%u before:%u", 
			badPduDesc->firstPktDesc->visitCount, flowDesc->queueLen);
		#endif
		flowDesc->releasePktDescriptors(badPduDesc->firstPktDesc, 
			badPduDesc->lastPktDesc, true);
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC)
		printf(" after:%u tcp_seq:%u\n", flowDesc->queueLen, 
			badPduDesc->firstPktDesc->tcpSeqNum);
		badPduDesc->print();
		#endif
		passBadPdu(badPduDesc);
		return true;
	}
	return false;
}

bool
RpcParser::gcPktsByTimestamp(FlowDescriptorTcpRpc *flowDesc, 
	time_t ts)
{
	PduDescriptor *badPduDesc;
	badPduDesc = constructBadPduByTime(flowDesc, ts);
	if (badPduDesc != NULL) {
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC)
		printf("===============\n");
		printf("GC3: ts:%lu badPduDesc->firstPktDesc->ts:%lu before:%u", 
			ts, badPduDesc->firstPktDesc->pcapHeader.ts.tv_sec,
			flowDesc->queueLen);
		#endif
		flowDesc->releasePktDescriptors(badPduDesc->firstPktDesc, 
			badPduDesc->lastPktDesc, true);
		#if CHRON_DEBUG(CHRONICLE_DEBUG_GC)
		printf(" after:%u tcp_seq:%u\n", flowDesc->queueLen, 
			badPduDesc->firstPktDesc->tcpSeqNum);
		badPduDesc->print();
		#endif
		passBadPdu(badPduDesc);
		return true;
	}
	return false;
}

bool
RpcParser::isRpcConnection(PacketDescriptor *pktDesc)
{
	// fixed RPC ports
	if (pktDesc->srcPort == NFS_PORT || pktDesc->destPort == NFS_PORT)
		return true;
	/*
	if (pktDesc->srcPort == SUNRPC_PORT || pktDesc->destPort == SUNRPC_PORT)
		return true;
	*/
	// TODO: non-fixed ports (i.e., ports replied by the portmapper)
	return false;	
}

void
RpcParser::gcFlowTable(PacketDescriptor *pktDesc)
{
	unsigned buckets;
	FlowDescriptorTcpRpc *rpcFlowDesc = NULL, *rpcReverseFlowDesc = NULL;

	if ((buckets = getNumFlowTableBucketsToGC(pktDesc)) > 0) {
		//printf("buckets:%u _gcBucket:%u\n", buckets, _gcBucket); // TODO: DEL
		for (uint32_t i = 0; i < buckets; i++, _gcBucket = (_gcBucket + 1) & 
				(FLOW_TABLE_DEFAULT_SIZE - 1)) {
			rpcFlowDesc = static_cast<FlowDescriptorTcpRpc *>
				(_flowTable->getBucketHead(_gcBucket));
			while (rpcFlowDesc != NULL) {
				// CALL flow descriptors will be ignored as they
				// get parsed when the corresponding REPLY 
				// descriptor is being garbage collected.
				if (rpcFlowDesc->sourcePort == NFS_PORT) {
					// parse the CALL packets
					rpcReverseFlowDesc = static_cast<FlowDescriptorTcpRpc *> 
						(rpcFlowDesc->flowDescriptorReverse);

					parseRpcPackets(rpcReverseFlowDesc, rpcFlowDesc);

					// garbage collect all the stale CALL packets
					if (rpcReverseFlowDesc->head != NULL) {
						gcPktsByTimestamp(rpcReverseFlowDesc, 
							pktDesc->pcapHeader.ts.tv_sec - FLOW_DESC_TIME_WINDOW);
					}

					// parse the REPLY packets
					parseRpcPackets(rpcFlowDesc, rpcReverseFlowDesc);

					// garbage collect all the stale REPLY packets
					if (rpcFlowDesc->head != NULL) {
						gcPktsByTimestamp(rpcFlowDesc, 
							pktDesc->pcapHeader.ts.tv_sec - FLOW_DESC_TIME_WINDOW);
					}

					// garbage collect unmatched calls
					rpcReverseFlowDesc->gcCallPdus(pktDesc);

					// remove the empty flow descriptors
					if (rpcFlowDesc->head == NULL && 
							rpcReverseFlowDesc->head == NULL &&
							rpcReverseFlowDesc->_callPduList.size() == 0) {
						unsigned bucketNum;
						_flowTable->lookupFlow(
							rpcReverseFlowDesc->sourceIP, 
							rpcReverseFlowDesc->destIP,
							rpcReverseFlowDesc->sourcePort,
							rpcReverseFlowDesc->destPort,
							rpcReverseFlowDesc->protocol, bucketNum);
						#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
						_goodPduCount += 
							rpcReverseFlowDesc->_readyPdusListSize;
						rpcReverseFlowDesc->printStats();
						rpcFlowDesc->printStats();
						#endif
						rpcReverseFlowDesc->passReadyPdus(_sink, this);
						_flowTable->removeFlowDesc(_gcBucket, rpcFlowDesc);
						_flowTable->removeFlowDesc(bucketNum, rpcReverseFlowDesc);
					}
				}
				rpcFlowDesc = static_cast<FlowDescriptorTcpRpc *>
					(rpcFlowDesc->next);
			}
		}
		_gcTick = pktDesc->pcapHeader.ts.tv_sec * 1000000UL + 
			pktDesc->pcapHeader.ts.tv_usec;
	}
}

unsigned 
RpcParser::getNumFlowTableBucketsToGC(PacketDescriptor *pktDesc)
{
	long interval = (long)(pktDesc->pcapHeader.ts.tv_sec) * 1000000 + 
		(long)(pktDesc->pcapHeader.ts.tv_usec) - _gcTick;
	if (interval < 0)
		return 0;
	/* TODO: DEL
	if (interval/1000) 
		printf("interval:%lu _gcTick: %lu\n", interval, _gcTick);
	*/
	return std::min<unsigned>(interval/1000000, 50/*FLOW_TABLE_DEFAULT_SIZE/200*/); 
}

#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
void 
RpcParser::getStats(uint64_t &goodPduCnt, uint64_t &badPduCnt,
		uint64_t &completePduCallCnt, uint64_t &completePduReplyCnt,
		uint64_t &completeHdrPduCallCnt, 
		uint64_t &completeHdrPduReplyCnt,
		uint64_t &unmatchedCallCnt, uint64_t &unmatchedReplyCnt) {
	goodPduCnt = _goodPduCount;
	badPduCnt = _badPduCount;
	completePduCallCnt = _completePduCallCount;
	completePduReplyCnt = _completePduReplyCount;
	completeHdrPduCallCnt = _completeHdrPduCallCount;
	completeHdrPduReplyCnt = _completeHdrPduReplyCount;
	unmatchedCallCnt = _unmatchedCallCount;
	unmatchedReplyCnt = _unmatchedReplyCount;
}
#endif

