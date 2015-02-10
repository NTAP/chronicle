// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef NFS_PARSER_H
#define NFS_PARSER_H

#include <linux/nfs.h>
#include <linux/nfs3.h>
#include "Process.h"
#include "ChronicleProcess.h"

#define NFS3_SET_TO_CLIENT_TIME					2
#define NFS3_COOKIEVERFSIZE 					8

class PduDescriptor;
class TcpStreamNavigator;
class OutputManager;

class NfsParser : public Process, public PduDescReceiver, 
		public ChronicleSource {
	public:
		NfsParser(ChronicleSource *src, uint32_t pipelineId, 
			OutputManager *outputManager);
		~NfsParser();
		void processRequest(ChronicleSource *src, PduDescriptor *pduDesc);
		void shutdown(ChronicleSource *src);
		void shutdownDone(ChronicleSink *sink);
		void processDone(ChronicleSink *sink);
		void setSink(PduDescReceiver *s) { _sink = s; }
		void parseNfsV3Pdu(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3GetattrCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3GetattrReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3SetattrCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3SetattrReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3LookupCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3LookupReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3AccessCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3AccessReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReadlinkCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReadlinkReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReadCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReadReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3WriteCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3WriteReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3CreateCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3CreateReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3MkdirCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3MkdirReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3SymlinkCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3SymlinkReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3MknodCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3MknodReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3RemoveRmdirCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3RemoveRmdirReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3RenameCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3RenameReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3LinkCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3LinkReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReaddirCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReaddirReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReaddirplusCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3ReaddirplusReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FsstatCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FsstatReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FsinfoCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FsinfoReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3PathconfCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3PathconfReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3CommitCall(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3CommitReply(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FileHandle(NfsV3PduDescriptor *nfsPduDesc);
		bool skipNfs3FileHandle(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3FileAttr(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3SetAttr(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3PostOpFileHandle(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3PostOpAttr(NfsV3PduDescriptor *nfsPduDesc);
		bool parseNfs3PrePostOpAttrs(NfsV3PduDescriptor *nfsPduDesc);
		std::string getId() { return std::to_string(_pipelineId); }

		static struct Nfs3OperationCounts {
			std::atomic<uint64_t> packets;
			std::atomic<uint64_t> getattr;
			std::atomic<uint64_t> setattr;
			std::atomic<uint64_t> lookup;
			std::atomic<uint64_t> access;
			std::atomic<uint64_t> readlink;
			std::atomic<uint64_t> read;
			std::atomic<uint64_t> write;
			std::atomic<uint64_t> create;
			std::atomic<uint64_t> mkdir;
			std::atomic<uint64_t> symlink;
			std::atomic<uint64_t> mknod;
			std::atomic<uint64_t> remove;
			std::atomic<uint64_t> rmdir;
			std::atomic<uint64_t> rename;
			std::atomic<uint64_t> link;
			std::atomic<uint64_t> readdir;
			std::atomic<uint64_t> readdirplus;
			std::atomic<uint64_t> fsstat;
			std::atomic<uint64_t> fsinfo;
			std::atomic<uint64_t> pathconf;
			std::atomic<uint64_t> commit;
		} nfs3OpCounts;

	private:
		class MsgBase;
		class MsgProcessRequest;
		class MsgShutdownProcess;
		class MsgKillNfsParser;
		class MsgProcessDone;
		
		void doProcessRequest(PduDescriptor *pduDesc);
		void doShutdownProcess();
		void doKillNfsParser();
		void doHandleProcessDone();

		/// The source process in the Chronicle pipeline
		ChronicleSource *_source;
		/// The pipeline іn which this parser belongs
		uint32_t _pipelineId;
		/// The sink process in the Chronicle pipeline
		PduDescReceiver *_sink;
		/// The TCP stream navigator
		TcpStreamNavigator *_streamNavigator;
		/// The output module
		OutputManager *_outputManager;
		#if CHRON_DEBUG(CHRONICLE_DEBUG_STAT)
		uint64_t _parsablePdus;
		uint64_t _unparsablePdus;
		uint64_t _failedPdus;
		#endif
};

#endif // NFS_PARSER_H

