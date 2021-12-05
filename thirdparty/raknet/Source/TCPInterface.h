/// \file
/// \brief A simple TCP based server allowing sends and receives.  Can be connected by any TCP client, including telnet.
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

#ifndef __SIMPLE_TCP_SERVER
#define __SIMPLE_TCP_SERVER

#ifdef _XBOX360
#include "Console1Includes.h"
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h> // fd_set
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
/// Unix/Linux uses ints for sockets
typedef unsigned int SOCKET;
#endif

#include "RakMemoryOverride.h"
#include "DS_List.h"
#include "RakNetTypes.h"
#include "SingleProducerConsumer.h"
#include "Export.h"
#include "RakThread.h"
#include "DS_Queue.h"
#include "SimpleMutex.h"
#include "RakNetDefines.h"

struct RemoteClient;

#if defined(OPEN_SSL_CLIENT_SUPPORT)
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/// \internal
/// \brief As the name says, a simple multithreaded TCP server.  Used by TelnetTransport
class RAK_DLL_EXPORT TCPInterface : public RakNet::RakMemoryOverride
{
public:
	TCPInterface();
	~TCPInterface();

	/// Starts the TCP server on the indicated port
	bool Start(unsigned short port, unsigned short maxIncomingConnections);

	/// Stops the TCP server
	void Stop(void);

	/// Connect to the specified host on the specified port
	SystemAddress Connect(const char* host, unsigned short remotePort, bool block=true);

#if defined(OPEN_SSL_CLIENT_SUPPORT)
	/// Start SSL on an existing connection, notified with HasCompletedConnectionAttempt
	void StartSSLClient(SystemAddress systemAddress);

	/// Was SSL started on this socket?
	bool IsSSLActive(SystemAddress systemAddress);
#endif

	/// Sends a byte stream
	void Send( const char *data, unsigned length, SystemAddress systemAddress );

	/// Returns data received
	Packet* Receive( void );

	/// Disconnects a player/address
	void CloseConnection( SystemAddress systemAddress );

	/// Deallocates a packet returned by Receive
	void DeallocatePacket( Packet *packet );

	/// Has a previous call to connect succeeded? Only used if block==false in the call to Connect
	/// \return UNASSIGNED_SYSTEM_ADDRESS = no. Anything else means yes.
	SystemAddress HasCompletedConnectionAttempt(void);

	/// Has a previous call to connect failed? Only used if block==false in the call to Connect
	/// \return UNASSIGNED_SYSTEM_ADDRESS = no. Anything else means yes.
	SystemAddress HasFailedConnectionAttempt(void);

	/// Queued events of new connections
	SystemAddress HasNewConnection(void);

	/// Queued events of lost connections
	SystemAddress HasLostConnection(void);
protected:

	bool isStarted, threadRunning;
	SOCKET listenSocket;

	// Assuming remoteClients is only used by one thread!
	DataStructures::List<RemoteClient*> remoteClients;
	// Use this thread-safe queue to add to remoteClients
	DataStructures::Queue<RemoteClient*> remoteClientsInsertionQueue;
	SimpleMutex remoteClientsInsertionQueueMutex;
	DataStructures::SingleProducerConsumer<Packet> outgoingMessages, incomingMessages;
	DataStructures::SingleProducerConsumer<SystemAddress> newConnections, lostConnections, requestedCloseConnections;
	DataStructures::SingleProducerConsumer<RemoteClient*> newRemoteClients;
	SimpleMutex completedConnectionAttemptMutex, failedConnectionAttemptMutex;
	DataStructures::Queue<SystemAddress> completedConnectionAttempts, failedConnectionAttempts;

	DataStructures::List<SOCKET> blockingSocketList;
	SimpleMutex blockingSocketListMutex;

	friend RAK_THREAD_DECLARATION(UpdateTCPInterfaceLoop);
	friend RAK_THREAD_DECLARATION(ConnectionAttemptLoop);

	void DeleteRemoteClient(RemoteClient *remoteClient, fd_set *exceptionFD);
	void InsertRemoteClient(RemoteClient* remoteClient);
	SOCKET SocketConnect(const char* host, unsigned short remotePort);

	struct ThisPtrPlusSysAddr
	{
		TCPInterface *tcpInterface;
		SystemAddress systemAddress;
		bool useSSL;
	};

#if defined(OPEN_SSL_CLIENT_SUPPORT)
	SSL_CTX* ctx;
	SSL_METHOD *meth;
	DataStructures::SingleProducerConsumer<SystemAddress> startSSL;
	DataStructures::List<SystemAddress> activeSSLConnections;
#endif
};

/// Stores information about a remote client.
struct RemoteClient
{
	RemoteClient() {
#if defined(OPEN_SSL_CLIENT_SUPPORT)
		ssl=0;
#endif
	}
	SOCKET socket;
	SystemAddress systemAddress;

#if defined(OPEN_SSL_CLIENT_SUPPORT)
	SSL*     ssl;
	void InitSSL(SSL_CTX* ctx, SSL_METHOD *meth);
	void DisconnectSSL(void);
	void FreeSSL(void);
	void Send(const char *data, unsigned int length);
	int Recv(char *data, const int dataSize);
#else
	void Send(const char *data, unsigned int length);
	int Recv(char *data, const int dataSize);
#endif
};

#endif
