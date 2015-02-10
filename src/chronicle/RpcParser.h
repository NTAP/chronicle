// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef RPC_PARSER_H
#define RPC_PARSER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <pcap.h>
#include <sys/time.h>
#include "Process.h"
#include "ChronicleProcess.h"
#include "FlowDescriptor.h"

using boost::multi_index_container;
using namespace boost::multi_index;

// RPC 4.0 portmapper port
#define SUNRPC_PORT							111					
// NFSv3/NFSv2 server port
#define NFS_PORT							2049
// Min RPC call header length (8B credentials/verifier)
#define MIN_RPC_CALL_HEADER_LEN				44
// Min RPC replay header length (8B credentials/verifier)
#define MIN_RPC_REPLY_HEADER_LEN			28
// RPC call
#define RPC_CALL							0
// RPC reply
#define RPC_REPLY							1
// RPC version
#define RPC_VERSION							2
// RPC portmap program
#define RPC_PORTMAP_PROGRAM					100000
// Bad PDU marker
#define RPC_BAD_PDU							666

class PacketDescriptor;
class PduDescriptor;
class FlowDescriptorIPv4;
class FlowTable;
class PacketBufferPool;
class TcpStreamNavigator;
class RpcParser;

/**
 * A class to implement an RPC TCP flow descriptor
 */
class FlowDescriptorTcpRpc : public FlowDescriptorTcp {
	public:
		struct xid_extractor {
			//http://www.boost.org/doc/libs/1_55_0/libs/multi_index/doc/tutorial/key_extraction.html
			typedef unsigned result_type;
			result_type& operator() (PduDescriptor *pduDesc) const { return pduDesc->rpcXid; }
		};

		typedef multi_index_container<
			PduDescriptor *,
			indexed_by<
				sequenced<>, //FIFO order
				hashed_unique<xid_extractor> // xid lookup
			>
		> PduList;

		// for keeping track of the insertion order
		typedef PduList::nth_index<0>::type PduListByInsert;
		// for fast lookup based on the xid value
		typedef PduList::nth_index<1>::type PduListByXid;

		FlowDescriptorTcpRpc(uint32_t sourceIP, uint32_t destIP, 
			uint16_t sourcePort, uint16_t destPort, uint8_t protocol,
			RpcParser *parser);
		~FlowDescriptorTcpRpc();
		/**
		 * inserts a call PDU into the list of outstanding PDUs
		 */
		void insertCallPdu(PduDescriptor *pduDesc, bool shutdown = false);
		/**
		 * given an xid, it finds the corresponding call PDU for a reply PDU
		 * param[in] xid The xid of the call PDU
		 * returns The PDU descriptor of the call PDU
		 */
		PduDescriptor *getCallPdu(uint32_t xid);
		/**
		 * returns true if a specific call PDU has already been processed
		 * param[in] xid The xid of the call PDU
		 * returns true if xid is in the call PDU list
		 */
		bool hasSeenCallPdu(uint32_t xid);
		/**
		 * inserts a call/reply PDU pair to the list of PDUs ready to be passed
		 */
		void insertReadyPdus(PduDescriptor *pduDesc, uint16_t num);
		/**
		 * passes ready PDUs to the next process in the pipeline
		 */
		void passReadyPdus(PduDescReceiver *sink, RpcParser *src);
		/**
		 * passes call PDUs from the call list to the ready list
		 */
		void passCallPdus();
		/**
		 * garbage collects unmatched call PDUs based on timestamp
		 */
		void gcCallPdus(PacketDescriptor *pktDesc);
		/**
		 * increases visitCount for certain number of packets
		 * param[in] the number of packets from head whose count may increase
		 * param[in] the new visitCount for those packets
		 */
		void increaseVisitCount(uint32_t numPkts, uint16_t newCount);
		/**
		 * increases visitCount for packets up to a point
		 * param[in] the last packet whose count may increase
		 * param[in] the new visitCount for those packets
		 */
		void increaseVisitCount(PacketDescriptor *pktDesc, uint16_t newCount);
		/**
		 * prints stats for the descriptor
		 */
		void printStats();

		/// The head for ready PDUs list
		PduDescriptor *_readyPdusListHead;
		/// The tail for ready PDUs list
		PduDescriptor *_readyPdusListTail;
		/// The current size of the ready PDUs list
		uint32_t _readyPdusListSize;
		/// Call PDU list
		PduList _callPduList;
		/// Call PDU list indexed by xid
		PduListByXid *_callPduListByXid; 
		/// Call PDU list indexed by insertion order
		PduListByInsert *_callPduListByInsert;
		/// The number of unmatched calls
		uint64_t _unmatchedCallCount;
		/// Corresponding RpcParser
		RpcParser *_rpcParser;
};

/**
 * A class to implement an RPC parser
 */
class RpcParser : public Process, public PacketDescReceiver, 
		public ChronicleSource {
	public:
		/// complete PDU types
		typedef enum action {
			ACTION_NONE,
			ACTION_INSERT_CALL,
			ACTION_PASS_REPLY,
		} CompletePduAction;
		RpcParser(ChronicleSource *src, unsigned pipelineId);
		~RpcParser();
		void processRequest(ChronicleSource *src, PacketDescriptor *pktDesc);
		void shutdown(ChronicleSource *src);
		void shutdownDone(ChronicleSink *sink);
		void processDone(ChronicleSink *sink);
		void setSink(PduDescReceiver *s) { _sink = s; }
		/**
		 * parses RPC packets for a given flow descriptor
		 * @param[in] rpcFlowDesc The flow descriptor whose packets we are parsing
		 * @param[in] rpcReverseFlowDesc The reverse flow descriptor
		 */
		void parseRpcPackets(FlowDescriptorTcpRpc *rpcFlowDesc,
			FlowDescriptorTcpRpc *rpcReverseFlowDesc);
		/**
		 * looks for a PDU in the flow table
		 * @param[in] flowDesc The flow descriptor whose packets we are scanning
		 * @param[out] compPduAction The type of action found PDU
		 * @param[inout] pktDesc The packet descriptor from which we start scanning
		 * @returns a call/reply PDU pair
		 */
		PduDescriptor *findPdu(FlowDescriptorTcpRpc *flowDesc, 
			CompletePduAction &compPduAction, PacketDescriptor **pktDesc=NULL);
		/**
		 * looks for an NFS PDU and if found returns one
		 * @param[in] flowDesc The flow descriptor whose packets we are scanning
		 * @param[in] firstPktDesc The packet descriptor where RPC header is found
		 * @param[in] firstProgPktDesc The packet descriptor where RPC prog is found
		 * @param[in] rpcHeaderOffset The offset where RPC header starts
		 * @param[in] rpcProgOffset The offset where RPC program starts
		 * @param[in] pduLen The length of this PDU
		 * @param[in] xid The xid of this PDU
		 * @param[in] progVersion The program version within RPC PDU
		 * @param[in] progProc The procedure number for the RPC program
		 * @param[in] RPC accept state for reply PDUs
		 * @param[in] msgType The type of PDU (i.e., call or reply)
		 */
		PduDescriptor *findNfsPdu(FlowDescriptorTcpRpc *flowDesc, 
			PacketDescriptor *firstPktDesc,
			PacketDescriptor *firstProgPktDesc,
			uint16_t rpcHeaderOffset, uint16_t rpcProgOffset, 
			uint32_t pduLen, uint32_t xid, uint32_t progVersion, 
			uint32_t progProc, uint16_t acceptState, uint8_t msgType);
		/**
		 * constructs a PDU and detaches the PDU packets from the flow table
		 * @param[in] flowDesc The flow descriptor whose packets we are detaching
		 * @param[in] pduDesc The PDU descriptor that we're constructing and detaching
		 */
		PduDescriptor *constructAndDetachPdu(FlowDescriptorTcpRpc *flowDesc,
			PduDescriptor *pduDesc);
		/**
		 * constructs and returns a PDU
		 * @param[in] the flow descriptor for the PDU we are constructing
		 * @param[in] pduDesc The descriptor for the PDU we are constructing
		 * @returns true when a PDU can fully be constructed
		 */
		bool constructPdu(FlowDescriptorTcpRpc *flowDesc, PduDescriptor *pduDesc);
		/**
		 * constructs a "bad" PDU out of all packets that don't belong to a valid PDU
		 * @param[in] firstPktDesc first packet descriptor for the bad PDU
		 * @param[in] lastPktDesc last packet descriptor for the bad PDU
		 */
		PduDescriptor *constructBadPdu(PacketDescriptor *firstPktDesc, 
			PacketDescriptor *lastPktDesc);
		/**
		 * constructs a "bad" PDU of certain size from the head
		 * @param[in] flowDesc flow descriptor for the bad PDU
		 * @param[in] size the number of packets in the bad PDU
		 */
		PduDescriptor *constructBadPduBySize(FlowDescriptorTcpRpc *flowDesc, 
			int size);
		/**
		 * constructs a "bad" PDU out of all the packets that have certain visit counts
		 * @param[in] flowDesc flow descriptor
		 * @param[in] pktDesc the packet that is part of bad PDU
		 * @param[in] visitCount the threshold that determines which packets make up the bad PDU
		 */
		PduDescriptor *constructBadPduByVisit(FlowDescriptorTcpRpc *flowDesc, 
			PacketDescriptor *pktDesc, uint16_t visitCount);
		/**
		 * constructs a "bad" PDU out of all the packets that have certain timestamps
		 * @param[in] flowDesc flow descriptor
		 * @param[in] ts the timestamp that determines which packets make up the bad PDU
		 */
		PduDescriptor *constructBadPduByTime(FlowDescriptorTcpRpc *flowDesc, 
			time_t ts);
		/**
		 * passes a bad PDU to the next stage in the pipeline
		 * @param[in] pduDesc The descriptor we are passing
		 */
		void passBadPdu(PduDescriptor *pduDesc);
		/**
		 * scans for RPC header given a flow descriptor and if successful may
		 * modify the flow table.
		 * @param[in] flowDesc The flow whose packets are being scanned
		 * @param[out] pktDesc The packet where the successful scan started (actually it's the new head due to garbage collection)
		 * @returns true if an RPC header is found
		 */
		bool scanForRpcHeader(FlowDescriptorTcpRpc *flowDesc, 
			PacketDescriptor **rpcHdrPktDesc);
		/**
		 * garbage collects packets above some visit count
		 * @param[in] flowDesc The flow whose packets we are garbage collecting
		 * @param[in] pktDesc The packet where we start our garbage collection
		 * @param[in] visitCount The threhsold for garbage collection
		 * @returns true if we garbage collected any packets
		 */
		bool gcPktsByVisitCount(FlowDescriptorTcpRpc *flowDesc, 
			PacketDescriptor *pktDesc, uint16_t visitCount);
		/**
		 * garbage collects packets older than some timestamp
		 * @param[in] flowDesc The flow whose packets we are garbage collecting
		 * @param[in] ts The timestamp for garbage collection
		 * @returns true if we garbage collected any packets
		 */
		bool gcPktsByTimestamp(FlowDescriptorTcpRpc *flowDesc, 
			time_t ts);
		/**
		 * garbage collects packets from head up to some packet
		 * @param[in] flowDesc The flow whose packets we are garbage collecting
		 * @param[in] pktDesc The packet up to which we're garbage collecting
		 * @returns true if we garbage collected any packets
		 */
		bool gcPktsUpto(FlowDescriptorTcpRpc *flowDesc, 
			PacketDescriptor *pktDesc);
		/**
		 * validates if a packet belongs to an RPC connection
		 * @param[in] pktDesc The packet descriptor for incoming packet
		 * @returns true if the packet belongs to an RPC connection
		 */
		bool isRpcConnection(PacketDescriptor *pktDesc);
		/**
		 * garbage collects the flow table
		 * @param[in] pktDesc The packet descriptor whose timestamp is used for
		 * garbage collection.
		 */
		void gcFlowTable(PacketDescriptor *pktDesc);
		/**
		 * returns the number of buckets in the flow table that need to be
		 * garbage collected. In time-based garbage collection, we garbage
		 * collect n buckets if nx10 msec have passed.
		 * @param[in] pktDesc The packet descriptor whose timestamp is used
		 * to know it's time to garbage collect
		 * @returns the number of buckets
		 */
		unsigned getNumFlowTableBucketsToGC(PacketDescriptor *pktDesc);
		/**
		 * returns the unique Process ID
		 */
		std::string getId() { return std::to_string(_pipelineId); }
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		void getStats(uint64_t &goodPduCount, uint64_t &badPduCount,
			uint64_t &completePduCallCount, 
			uint64_t &comletePduReplyCount,
			uint64_t &completeHdrPduCallCount,
			uint64_t &completeHdrPduReplyCount,
			uint64_t &unmatchedCallCount, 
			uint64_t &unmatchedReplyCount);
		void incrementUnmatchedCalls(uint64_t count) 
			{ _unmatchedCallCount += count; }
		#endif

	private:
		class MsgBase;
		class MsgProcessRequest;
		class MsgShutdownProcess;
		class MsgKillRpcParser;
		class MsgProcessDone;
		
		void doProcessRequest(PacketDescriptor *pktDesc);
		void doShutdownProcess();
		void doKillRpcParser();
		void doHandleProcessDone();

		/// The source process in the Chronicle pipeline
		ChronicleSource *_source;
		/// The pipeline in which this parser belongs
		uint32_t _pipelineId;
		/// The sink process in the Chronicle pipeline
		PduDescReceiver *_sink;
		/// The table holding all the flows and packets for this parser
		FlowTable *_flowTable;
		/// A reference to packet buffer pool for releasing useless packets
		PacketBufferPool *_bufPool;
		/// TCP stream navigator
		TcpStreamNavigator *_streamNavigator;
		/// garbage collection tick (in microsecond)
		uint64_t _gcTick;
		/// flow table bucket to garbage collect
		uint32_t _gcBucket;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		/// The count of good PDUs
		uint64_t _goodPduCount;
		/// The count of bad PDUs
		uint64_t _badPduCount;
		/// The print count limit for good PDUs
		uint64_t _goodPduPrintLimit;
		/// The print count limit for bad PDUs
		uint64_t _badPduPrintLimit;
		/// The count of PDU_COMPLETE call PDUs
		uint64_t _completePduCallCount;
		/// The count of PDU_COMPLETE reply PDUs
		uint64_t _completePduReplyCount;
		/// The count of PDU_COMPLETE_HEADER call PDUs
		uint64_t _completeHdrPduCallCount;
		/// The count of PDU_COMPLETE_HEADER reply PDUs
		uint64_t _completeHdrPduReplyCount;
		/// The number of unmatched calls for this parser
		uint64_t _unmatchedCallCount;
		/// The minimum number of unmatched replies for this parser
		uint64_t _unmatchedReplyCount;
		/// The number of scanned RPC call headers
		uint64_t _scannedRpcCallHdrs;
		/// The number of scanned RPC reply headers
		uint64_t _scannedRpcReplyHdrs;
		/// The number of forced RPC reply scans 
		// (when queueLen > RPC_PARSER_REPLY_PARSE_THRESH)
		uint64_t _forcedRpcReplyScanCount;
		/// The number of forced RPC reply scans 
		// (when queueLen > RPC_PARSER_REPLY_PARSE_THRESH * 2)
		uint64_t _forcedGcCount;
		#endif
};

#endif //RPC_PARSER_H
