#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "Message.h"
#include "Chronicle.h"
#include "CommandChannel.h"

class CommandChannel::MsgBase : public Message {
	protected:
		CommandChannel *_channel;

	public:
		MsgBase(CommandChannel *channel) : _channel(channel) { }
		virtual ~MsgBase() { }
};

class CommandChannel::MsgProcessChannel : public MsgBase {
	public:
		MsgProcessChannel(CommandChannel *channel) : MsgBase(channel) { }
		void run() { 
			_channel->doProcessChannel(); }
};

class CommandChannel::MsgCloseChannel : public MsgBase {
	public:
		MsgCloseChannel(CommandChannel *channel) : MsgBase(channel) { }
		void run() { 
			_channel->doCloseChannel(); }
};

class CommandChannel::MsgChannelDead : public MsgBase {
	public:
		MsgChannelDead(CommandChannel *channel) : MsgBase(channel) { }
		void run() { 
			_channel->doNotifyChannelDead(); }
};

class CommandChannel::MsgKillChannel : public MsgBase {
	public:
		MsgKillChannel(CommandChannel *channel) : MsgBase(channel) { }
		void run() { 
			_channel->doKillChannel(); }
};

class CommandChannel::ChannelReadyCb : public FDWatcher::FdCallback {
	private:
		CommandChannel *_channel;
	
	public:
		ChannelReadyCb(CommandChannel *channel) : _channel(channel) { }
		void operator()(int fd, FDWatcher::Events event) {
			_channel->processChannel();
		}
};

CommandChannel::CommandChannel(Chronicle *supervisor, int fd, 
	FDWatcher::Events events) 
    : Process("CommandChannel"), _supervisor(supervisor), _fd(fd), _events(events)
{
	_currentToken.str("");
	_channelDone = false;
	fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK);	
	_channelCb = new ChannelReadyCb(this);
	FDWatcher::getFDWatcher()->registerFdCallback(_fd, _events, _channelCb);
}

CommandChannel::~CommandChannel()
{
	delete _channelCb;
}

void
CommandChannel::processChannel()
{
	enqueueMessage(new MsgProcessChannel(this));
}

void
CommandChannel::doProcessChannel()
{
	char buf[_maxCommandLength];
	int bytes, i;

	if (_channelDone) return ;

	do {
		i = 0;
        bytes = read(_fd, buf, sizeof(buf));
		// parsing the command
		while (bytes > 0 && i < bytes && !_channelDone) {
			switch(buf[i]) {
				case ' ':
					if (_currentToken.str().compare(""))
						_tokens.push_back(_currentToken.str());
					_currentToken.str("");
					break;
				case '\n':
					if (_currentToken.str().compare(""))
						_tokens.push_back(_currentToken.str());
					issueCommand();
					_currentToken.str("");
					_tokens.clear();
					if (!_channelDone) 
						FDWatcher::getFDWatcher()->registerFdCallback(_fd, 
							_events, _channelCb);
					break;
				default:
					_currentToken << buf[i];
			}
			i++;
		}
    } while(bytes > 0 && !_channelDone);
    if (bytes == 0) { 
        _channelDone = true;
		_supervisor->handleCommandChannel(OP_CLOSED_CHANNEL, _tokens);
    } else if (bytes < 0) {
        if (EAGAIN != errno && EWOULDBLOCK != errno) {
			_channelDone = true;
			_supervisor->handleCommandChannel(OP_BAD_CHANNEL, _tokens);
		}
    }
}

void
CommandChannel::closeChannel()
{
	enqueueMessage(new MsgCloseChannel(this));
}

void
CommandChannel::doCloseChannel()
{
	if (!_channelDone) {
		_channelDone = true;
		FDWatcher::getFDWatcher()->clearFdCallback(_fd);
	}
	enqueueMessage(new MsgChannelDead(this));
}

void
CommandChannel::doNotifyChannelDead()
{
	_supervisor->handleDeadChannel();
}

void
CommandChannel::killChannel()
{
	enqueueMessage(new MsgKillChannel(this));
}

void
CommandChannel::doKillChannel()
{
	close(_fd);
	exit();
}

void
CommandChannel::issueCommand()
{
	int command;
	std::string tmp = _tokens.front();
	if (!tmp.compare("GETSTAT")) {
		command = OP_INTERFACE_STAT;
		_tokens.pop_front();
		if (_tokens.size() != 1) {
			usage();
			return ;
		}
	} else if (!tmp.compare("CLOSE")) {
		command = OP_INTERFACE_CLOSE;
		_tokens.pop_front();
		if (_tokens.size() != 1) {
			usage();
			return ;
		}
	} else if (!tmp.compare("QUIT")) {
		_tokens.pop_front();
		command = OP_QUIT;
		_channelDone = true;
	} else if (!tmp.compare("ANALYTICS")) {
		_tokens.pop_front();
		command = OP_ANALYTICS;
	} else {
		usage();
		return ;
	}
		
	std::list<std::string> args(_tokens);
	_supervisor->handleCommandChannel(command, args);
}

void
CommandChannel::usage()
{
	std::cerr << FONT_RED 
		<< "Usage:\n"
		<< "\tGETSTAT\n"
		<< "\tCLOSE interface_name|ALL\n"
		<< "\tQUIT\n"
		<< FONT_DEFAULT;
}

