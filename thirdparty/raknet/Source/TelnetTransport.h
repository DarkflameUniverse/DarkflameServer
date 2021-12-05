/// \file
/// \brief Contains TelnetTransport , used to supports the telnet transport protocol.  Insecure
///
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.jenkinssoftware.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.

#ifndef __TELNET_TRANSPORT
#define __TELNET_TRANSPORT

#include "TransportInterface.h"
#include "DS_List.h"
#include "Export.h"
class TCPInterface;
struct TelnetClient;

/// \brief Use TelnetTransport to easily allow windows telnet to connect to your ConsoleServer
/// To run Windows telnet, go to your start menu, click run, and in the edit box type "telnet <IP>" where <IP> is the ip address
/// of your ConsoleServer (most likely the same IP as your game).
/// This implementation always echos commands.
class RAK_DLL_EXPORT TelnetTransport : public TransportInterface
{
public:
	TelnetTransport();
	virtual ~TelnetTransport();
	bool Start(unsigned short port, bool serverMode);
	void Stop(void);
	void Send( SystemAddress systemAddress, const char *data, ... );
	void CloseConnection( SystemAddress systemAddress );
	Packet* Receive( void );
	void DeallocatePacket( Packet *packet );
	SystemAddress HasNewConnection(void);
	SystemAddress HasLostConnection(void);
	CommandParserInterface* GetCommandParser(void);
	void SetSendSuffix(const char *suffix);
	void SetSendPrefix(const char *prefix);
protected:

	struct TelnetClient
	{
		SystemAddress systemAddress;
		char textInput[REMOTE_MAX_TEXT_INPUT];
		unsigned cursorPosition;
	};

	TCPInterface *tcpInterface;
	void AutoAllocate(void);
	bool ReassembleLine(TelnetTransport::TelnetClient* telnetClient, unsigned char c);

	// Crap this sucks but because windows telnet won't send line at a time, I have to reconstruct the lines at the server per player
	DataStructures::List<TelnetClient*> remoteClients;

	char *sendSuffix, *sendPrefix;

};

#endif
