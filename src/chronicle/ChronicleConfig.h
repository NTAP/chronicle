// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef CHRONICLE_CONFIG_H
#define CHRONICLE_CONFIG_H

#include <string>

/* ====================== *
 * Chronicle debug macros *
 * ====================== */
// Flags for printing debug messages
#define CHRONICLE_DEBUG_NETWORK					(1)
#define CHRONICLE_DEBUG_RPC						(1 << 1)
#define CHRONICLE_DEBUG_NFS						(1 << 2)
#define CHRONICLE_DEBUG_GC						(1 << 3)
#define CHRONICLE_DEBUG_VALGRIND				(1 << 4)
#define CHRONICLE_DEBUG_BUFPOOL					(1 << 5)
#define CHRONICLE_DEBUG_FLOWTABLE				(1 << 6)
#define CHRONICLE_DEBUG_STAT					(1 << 7)
#define CHRONICLE_DEBUG_CACHE					(1 << 8)
#define CHRONICLE_DEBUG_PKTLOSS					(1 << 9)
#define CHRONICLE_DEBUG_NETWORK_STAT			(1 << 10)
#define CHRONICLE_DEBUG_LEAK					(CHRONICLE_DEBUG_FLOWTABLE | \
												 CHRONICLE_DEBUG_BUFPOOL | \
												 CHRONICLE_DEBUG_STAT | \
												 CHRONICLE_DEBUG_RPC | \
												 CHRONICLE_DEBUG_GC)
#define CHRONICLE_DEBUG_PROFILE					(CHRONICLE_DEBUG_FLOWTABLE | \
												 CHRONICLE_DEBUG_BUFPOOL | \
												 CHRONICLE_DEBUG_STAT)
#define CHRONICLE_DEBUG_STAT_ALL				(CHRONICLE_DEBUG_STAT | \
												 CHRONICLE_DEBUG_NETWORK_STAT)

//#define CHRONICLE_DEBUG							0
#define CHRONICLE_DEBUG							CHRONICLE_DEBUG_STAT
#define CHRON_DEBUG(mode)						(CHRONICLE_DEBUG & (mode))

/* ======================== *
 * Chronicle pipline macros *
 * ======================== */
// Chronicle pipeline modes
#define NFS_PIPELINE							0 /* NFS parsing pipeline */
#define PCAP_PIPELINE							1 /* Pcap with no parsing */
// Default number of Chronicle pipelines
#define DEFAULT_PIPELINE_NUM					1
// Max number of Chronicle pipelines (has to be power of 2)
#define MAX_PIPELINE_NUM						256

/* ============================== *
 * Chronicle output module macros *
 * ============================== */
// Chronicle trace formats
#define NO_OUTPUT								0 /* No trace (analytics only) */
#define DS_OUTPUT								1 /* DataSeries output */
#define PCAP_OUTPUT								2 /* Pcap output */
// Default number of output modules
#define DEFAULT_OUTPUT_MODULE_NUM				1
// Max number of output modules
#define MAX_OUTPUT_MODULE_NUM					16
// Default trace file directory
#define TRACE_DIRECTORY							"/tmp"
extern std::string traceDirectory;

/* ===================== *
 * Chronicle buffer pool *
 * ===================== */
// Ethernet frame size
#define MAX_ETH_FRAME_SIZE_STAND				1536
#define MAX_ETH_FRAME_SIZE_JUMBO				9216

//CHRONICLE01 (2x10Gb NIC)
// number of standard sized packets in the buffer pool 
#define STAND_PKTS_IN_BUFFER_POOL				20000000
// number of jumbo sized packets in the buffer pool
#define JUMBO_PKTS_IN_BUFFER_POOL				1000000

/* //CHRONICLE02 (2x10Gb NIC)
#define STAND_PKTS_IN_BUFFER_POOL				10000000
// number of jumbo sized packets in the buffer pool
#define JUMBO_PKTS_IN_BUFFER_POOL				8000000 */

/* //(1x10Gb NIC)
#define STAND_PKTS_IN_BUFFER_POOL				40000000
// number of jumbo sized packets in the buffer pool
#define JUMBO_PKTS_IN_BUFFER_POOL				0 */

/* //SHUTTLE (2x10Gb NIC) - NO JUMBO
/// number of standard sized packets in the buffer pool
#define STAND_PKTS_IN_BUFFER_POOL				12000000
// number of jumbo sized packets in the buffer pool
#define JUMBO_PKTS_IN_BUFFER_POOL				0 */

/* //VM
/// number of standard sized packets in the buffer pool
#define STAND_PKTS_IN_BUFFER_POOL				120000
// number of jumbo sized packets in the buffer pool
#define JUMBO_PKTS_IN_BUFFER_POOL				0 */

/* ============= *
 * pcap defaults *
 * ============= */
// a pcap reader reads this many packets at a time 
#define PCAP_DEFAULT_BATCH_SIZE					100		
// wait so many millisecond before giving up
#define PCAP_DEFAULT_TIMEOUT_LEN 				0
// BPF filter length
#define PCAP_MAX_BPF_FILTER_LEN					500
// max trace file size (4GB)
#define PCAP_MAX_TRACE_SIZE						4294967296	

/* =============== *
 * netmap defaults *
 * =============== */
// netmap interface max batch size
#define NETMAP_MAX_BATCH_SIZE					4096			

/* =================== *
 * DataSeries defaults *
 * =================== */
// File size
#define DS_DEFAULT_FILE_SIZE                    4000000000ull
#define DS_DEFAULT_FILE_SIZE_SMALL               700000000ull
extern off64_t dsFileSize;
// Extent size
#define DS_DEFAULT_EXTENT_SIZE                  (16 * 1024 * 1024)
#define DS_DEFAULT_EXTENT_SIZE_SMALL            ( 8 * 1024 * 1024)
extern unsigned dsExtentSize;
// Are IP and Checksum extents enabled?
#define DS_DEFAULT_ENABLE_IP_CHECKSUM           true
extern bool dsEnableIpChecksum;

/* ============ *
 * Misc. macros *
 * ============ */
// Chronicle process IDs (used for tagging packet descriptors in debug mode)
#define PROCESS_NET_PARSER						2
#define PROCESS_RPC_PARSER						3
#define PROCESS_NFS_PARSER						4
#define PROCESS_DS_WRITER						5
// Max length of the commands parsed by CommandChannel
#define MAX_COMMAND_LENGTH						100
// Number of buckets in the flow table (has to be power of 2)
#define FLOW_TABLE_DEFAULT_SIZE					65536
// Batch size used by RpcParser to parse packets
#define RPC_PARSER_PACKETS_BATCH_SIZE			64
// Batch size used by RpcParser to pass ready PDUs
#define RPC_PARSER_PDUS_BATCH_SIZE				4 
// Threshold of "visited" packets for garbage collection 
// * It has to be >= RPC_PARSER_PACKETS_BATCH_SIZE)
// * 512 for deployments; 2000 for environments with relatively small out of order packets
#define RPC_PARSER_GC_THRESH					512 
// Threshold of "packet visits" to start scanning for an RPC header
#define RPC_PARSER_SCAN_THRESH					64
// Threshold of "packet visits" before constructing a COMPLETE_HEADER PDU
#define RPC_PARSER_COMPLETE_HDR_THRESH			(RPC_PARSER_GC_THRESH - 12)
// Threshold of unmatched calls for garbage collection
#define RPC_PARSER_UMATCH_CALL_GC_THRESH		(RPC_PARSER_REPLY_PARSE_THRESH << 1)
// Time window (in seconds) that RpcParser waits for calls to catch up with replies
#define RPC_PARSER_CALL_REPLY_SYNC_WAIT_TIME	5
// Threshold for the number of packets in a flow to prompt parsing RPC reply packets
// * Twice the ring size because the replies may be read before the calls 
#define RPC_PARSER_REPLY_PARSE_THRESH			16384 //8192
// Flag to prioritize PacketReader pthreads
#define PACKET_READER_PRIORITIZED				0
// Time window (in seconds) for holdings packets of a given flow
#define FLOW_DESC_TIME_WINDOW					30
// Max interface name length
#define MAX_INTERFACE_NAME_LEN					256
// Default console font color
#define FONT_DEFAULT							"\033[0;0m"
// Red font color
#define FONT_RED								"\033[0;31m"
// Green font color
#define FONT_GREEN								"\033[0;32m"
// Gold font color
#define FONT_GOLD								"\033[0;33m"
// Blue font color
#define FONT_BLUE								"\033[0;34m"		

#endif //CHRONICLE_CONFIG_H

