// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef COMMAND_CHANNEL_H
#define COMMAND_CHANNEL_H

#include <list>
#include <string>
#include <sstream>
#include "Process.h"
#include "Interface.h"
#include "FDWatcher.h"
#include "ChronicleConfig.h"

class Chronicle;

class CommandChannel : public Process {
	public:
		typedef enum command {
			OP_INTERFACE_STAT,
			OP_INTERFACE_CLOSE, 
			OP_QUIT,
			OP_CLOSED_CHANNEL,
			OP_BAD_CHANNEL,
			OP_ANALYTICS
		} Command;
		CommandChannel(Chronicle *supervisor, int fd, FDWatcher::Events events);
		~CommandChannel();
		/// called by the CommandChannel callback to process commands
		void processChannel();
		/// called by the supervisor to close the channel
		void closeChannel();
		/// called by the supervisor to kill the channel
		void killChannel();
		/// issues the command to the supervisor
		void issueCommand();
		/// usage info for CommandChannel
		void usage();

	private:
		class MsgBase;
		class MsgProcessChannel;
		class MsgCloseChannel;
		class MsgChannelDead;
		class MsgKillChannel;
		class ChannelReadyCb;

		void doProcessChannel();
		void doCloseChannel();
		void doNotifyChannelDead();
		void doKillChannel();

		/// pointer to the supervisor
		Chronicle *_supervisor;
		/// file descriptor for the channel
		int _fd;
		/// is channel done?
		bool _channelDone;
		/// events monitored by the channel
		FDWatcher::Events _events;
		/// tokens in a command
		std::list<std::string> _tokens;
		/// current token
		std::stringstream _currentToken;
		/// callback for the channel used by FDWatcher
		ChannelReadyCb *_channelCb;
		/// max command length
		static const int _maxCommandLength = MAX_COMMAND_LENGTH;
};

#endif //COMMAND_CHANNEL_H
