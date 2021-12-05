/// \file
/// \brief SocketLayer class implementation
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

#include "SocketLayer.h"
#include <assert.h>
#include "MTUSize.h"

#ifdef _WIN32
typedef int socklen_t;
#elif !defined(_PS3)
#include <string.h> // memcpy
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>  // error numbers
#include <stdio.h> // printf
#endif

#if defined(_PS3)
#if defined (_PS3_LOBBY)
// OK this is lame but you need to know the app port when sending to the external IP.
// I'm just assuming it's the same as our own because it would break the interfaces to pass it to SendTo directly.
static unsigned short HACK_APP_PORT;
#include "np/common.h"
#include <netdb.h>
#include <string.h>
#endif
#endif

#ifdef _WIN32
#elif defined(_PS3)
#define closesocket socketclose
#else
#define closesocket close
#include <unistd.h>
#endif

#include <stdio.h>

#include "ExtendedOverlappedPool.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

bool SocketLayer::socketLayerStarted = false;
#ifdef _WIN32
WSADATA SocketLayer::winsockInfo;
#endif
SocketLayer SocketLayer::I;

extern void ProcessNetworkPacket( const unsigned int binaryAddress, const unsigned short port, const char *data, const int length, RakPeer *rakPeer, unsigned connectionSocketIndex );
extern void ProcessPortUnreachable( const unsigned int binaryAddress, const unsigned short port, RakPeer *rakPeer );

#ifdef _DEBUG
#include <stdio.h>
#endif

SocketLayer::SocketLayer()
{
	if ( socketLayerStarted == false )
	{
#ifdef _WIN32

		if ( WSAStartup( MAKEWORD( 2, 2 ), &winsockInfo ) != 0 )
		{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
			DWORD dwIOError = GetLastError();
			LPVOID messageBuffer;
			FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
				( LPTSTR ) & messageBuffer, 0, NULL );
			// something has gone wrong here...
			printf( "WSAStartup failed:Error code - %d\n%s", dwIOError, messageBuffer );
			//Free the buffer.
			LocalFree( messageBuffer );
#endif
		}

#endif
		socketLayerStarted = true;
	}
}

SocketLayer::~SocketLayer()
{
	if ( socketLayerStarted == true )
	{
#ifdef _WIN32
		WSACleanup();
#endif

		socketLayerStarted = false;
	}
}

SOCKET SocketLayer::Connect( SOCKET writeSocket, unsigned int binaryAddress, unsigned short port )
{
	assert( writeSocket != (SOCKET) -1 );
	sockaddr_in connectSocketAddress;

	connectSocketAddress.sin_family = AF_INET;
	connectSocketAddress.sin_port = htons( port );
	connectSocketAddress.sin_addr.s_addr = binaryAddress;

	if ( connect( writeSocket, ( struct sockaddr * ) & connectSocketAddress, sizeof( struct sockaddr ) ) != 0 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) &messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "WSAConnect failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif
	}

	return writeSocket;
}
bool SocketLayer::IsPortInUse(unsigned short port)
{
	SOCKET listenSocket;
	sockaddr_in listenerSocketAddress;
	// Listen on our designated Port#
	listenerSocketAddress.sin_port = htons( port );
	listenSocket = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( listenSocket == (SOCKET) -1 )
		return true;
	// bind our name to the socket
	// Fill in the rest of the address structure
	listenerSocketAddress.sin_family = AF_INET;
	listenerSocketAddress.sin_addr.s_addr = INADDR_ANY;
	int ret = bind( listenSocket, ( struct sockaddr * ) & listenerSocketAddress, sizeof( listenerSocketAddress ) );
	closesocket(listenSocket);
	return ret <= -1;
}
SOCKET SocketLayer::CreateBoundSocket( unsigned short port, bool blockingSocket, const char *forceHostAddress )
{
	(void) blockingSocket;

	int ret;
	SOCKET listenSocket;
#if defined(_PS3) && defined (_PS3_LOBBY)
	sockaddr_in_p2p listenerSocketAddress;
	memset(&listenerSocketAddress, 0, sizeof(listenerSocketAddress));

	// Lobby version
	listenerSocketAddress.sin_port = htons(SCE_NP_PORT);
	HACK_APP_PORT = port; // Save the locally bound port
	listenerSocketAddress.sin_vport = htons(port);
	listenSocket = socket( AF_INET, SOCK_DGRAM_P2P, 0 );

	// Normal version as below
#else
	sockaddr_in listenerSocketAddress;
	// Listen on our designated Port#
	listenerSocketAddress.sin_port = htons( port );
	#if 0 // defined(_WIN32)
	listenSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
	#else
	listenSocket = socket( AF_INET, SOCK_DGRAM, 0 );
	#endif
#endif

	if ( listenSocket == (SOCKET) -1 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "socket(...) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif

		return (SOCKET) -1;
	}

	int sock_opt = 1;
	if ( setsockopt( listenSocket, SOL_SOCKET, SO_REUSEADDR, ( char * ) & sock_opt, sizeof ( sock_opt ) ) == -1 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "setsockopt(SO_REUSEADDR) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif
	}


	// This doubles the max throughput rate
	sock_opt=1024*256;
	setsockopt(listenSocket, SOL_SOCKET, SO_RCVBUF, ( char * ) & sock_opt, sizeof ( sock_opt ) );

#ifndef _PS3
	// This doesn't make much difference: 10% maybe
	// Not supported on console 2
	sock_opt=1024*16;
	setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, ( char * ) & sock_opt, sizeof ( sock_opt ) );
#endif

#ifdef _WIN32
	unsigned long nonblocking = 1;
	ioctlsocket( listenSocket, FIONBIO, &nonblocking );
#elif defined(_PS3)
	sock_opt=1;
	setsockopt(listenSocket, SOL_SOCKET, SO_NBIO, ( char * ) & sock_opt, sizeof ( sock_opt ) );
#else
	fcntl( listenSocket, F_SETFL, O_NONBLOCK );
#endif

	#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
	// If this assert hit you improperly linked against WSock32.h
	assert(IP_DONTFRAGMENT==14);
	#endif

	// TODO - I need someone on dialup to test this with :(
	// Path MTU Detection

//	if ( setsockopt( listenSocket, IPPROTO_IP, IP_DONTFRAGMENT, ( char * ) & sock_opt, sizeof ( sock_opt ) ) == -1 )
//	{
//#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
//		DWORD dwIOError = GetLastError();
//		LPVOID messageBuffer;
//		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
//			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
//		printf( "setsockopt(IP_DONTFRAGMENT) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
//		LocalFree( messageBuffer );
//#endif
//	}

	// Set broadcast capable
	sock_opt=1;
	if ( setsockopt( listenSocket, SOL_SOCKET, SO_BROADCAST, ( char * ) & sock_opt, sizeof( sock_opt ) ) == -1 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "setsockopt(SO_BROADCAST) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif

	}

	// Fill in the rest of the address structure
	listenerSocketAddress.sin_family = AF_INET;

	if (forceHostAddress && forceHostAddress[0])
	{
		listenerSocketAddress.sin_addr.s_addr = inet_addr( forceHostAddress );
	}
	else
	{
		listenerSocketAddress.sin_addr.s_addr = INADDR_ANY;
	}

	// bind our name to the socket
	ret = bind( listenSocket, ( struct sockaddr * ) & listenerSocketAddress, sizeof( listenerSocketAddress ) );

	if ( ret <= -1 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "bind(...) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#elif (defined(__GNUC__)  || defined(__GCCXML__) || defined(_PS3)) && !defined(__WIN32)
		switch (ret)
		{
		case EBADF:
			printf("bind(): sockfd is not a valid descriptor.\n"); break;
#ifndef _PS3
		case ENOTSOCK:
			printf("bind(): Argument is a descriptor for a file, not a socket.\n"); break;
#endif
		case EINVAL:
			printf("bind(): The addrlen is wrong, or the socket was not in the AF_UNIX family.\n"); break;
		case EROFS:
			printf("bind(): The socket inode would reside on a read-only file system.\n"); break;
		case EFAULT:
			printf("bind(): my_addr points outside the user's accessible address space.\n"); break;
		case ENAMETOOLONG:
			printf("bind(): my_addr is too long.\n"); break;
		case ENOENT:
			printf("bind(): The file does not exist.\n"); break;
		case ENOMEM:
			printf("bind(): Insufficient kernel memory was available.\n"); break;
		case ENOTDIR:
			printf("bind(): A component of the path prefix is not a directory.\n"); break;
		case EACCES:
			printf("bind(): Search permission is denied on a component of the path prefix.\n"); break;
#ifndef _PS3
		case ELOOP:
			printf("bind(): Too many symbolic links were encountered in resolving my_addr.\n"); break;
#endif
		default:
			printf("Unknown bind() error %i.\n", ret); break;
		}
#endif

		return (SOCKET) -1;
	}

	return listenSocket;
}

#if !defined(_XBOX360)
const char* SocketLayer::DomainNameToIP( const char *domainName )
{
	struct hostent * phe = gethostbyname( domainName );

	if ( phe == 0 || phe->h_addr_list[ 0 ] == 0 )
	{
		//cerr << "Yow! Bad host lookup." << endl;
		return 0;
	}

	struct in_addr addr;
	if (phe->h_addr_list[ 0 ]==0)
		return 0;

	memcpy( &addr, phe->h_addr_list[ 0 ], sizeof( struct in_addr ) );
	return inet_ntoa( addr );
}
#endif


void SocketLayer::Write( const SOCKET writeSocket, const char* data, const int length )
{
#ifdef _DEBUG
	assert( writeSocket != (SOCKET) -1 );
#endif

	send( writeSocket, data, length, 0 );
}

int SocketLayer::RecvFrom( const SOCKET s, RakPeer *rakPeer, int *errorCode, unsigned connectionSocketIndex )
{
	int len;
	char data[ MAXIMUM_MTU_SIZE ];

#if defined(_PS3) && defined (_PS3_LOBBY)
	sockaddr_in_p2p sa;
#else
	sockaddr_in sa;
#endif


	socklen_t len2 = sizeof( sa );
	sa.sin_family = AF_INET;

#ifdef _DEBUG
	data[ 0 ] = 0;
	len = 0;
	sa.sin_addr.s_addr = 0;
#endif

	if ( s == (SOCKET) -1 )
	{
		*errorCode = -1;
		return -1;
	}

#if defined (_WIN32) || !defined(MSG_DONTWAIT)
	const int flag=0;
#else
	const int flag=MSG_DONTWAIT;
#endif

	len = recvfrom( s, data, MAXIMUM_MTU_SIZE, flag, ( sockaddr* ) & sa, ( socklen_t* ) & len2 );

	// if (len>0)
	//  printf("Got packet on port %i\n",ntohs(sa.sin_port));

	if ( len == 0 )
	{
#ifdef _DEBUG
		printf( "Error: recvfrom returned 0 on a connectionless blocking call\non port %i.  This is a bug with Zone Alarm.  Please turn off Zone Alarm.\n", ntohs( sa.sin_port ) );
		assert( 0 );
#endif

		*errorCode = -1;
		return -1;
	}

	if ( len > 0 )
	// if ( len != SOCKET_ERROR )
	{
		unsigned short portnum;
//#if defined(_PS3) && defined (_PS3_LOBBY)
//		portnum = ntohs( sa.sin_vport );
//#else
		portnum = ntohs( sa.sin_port );
//#endif
		//strcpy(ip, inet_ntoa(sa.sin_addr));
		//if (strcmp(ip, "0.0.0.0")==0)
		// strcpy(ip, "127.0.0.1");
		ProcessNetworkPacket( sa.sin_addr.s_addr, portnum, data, len, rakPeer, connectionSocketIndex );

		return 1;
	}
	else
	{
		*errorCode = 0;


#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)

		DWORD dwIOError = WSAGetLastError();

		if ( dwIOError == WSAEWOULDBLOCK )
		{
			return SOCKET_ERROR;
		}
		if ( dwIOError == WSAECONNRESET )
		{
#if defined(_DEBUG)
//			printf( "A previous send operation resulted in an ICMP Port Unreachable message.\n" );
#endif


			unsigned short portnum=0;
			ProcessPortUnreachable(sa.sin_addr.s_addr, portnum, rakPeer);
			// *errorCode = dwIOError;
			return -1;
		}
		else
		{
#if defined(_DEBUG)
			if ( dwIOError != WSAEINTR )
			{
				LPVOID messageBuffer;
				FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
					( LPTSTR ) & messageBuffer, 0, NULL );
				// something has gone wrong here...
				printf( "recvfrom failed:Error code - %d\n%s", dwIOError, messageBuffer );

				//Free the buffer.
				LocalFree( messageBuffer );
			}
#endif
		}
#endif
	}

	return 0; // no data
}

#ifdef _MSC_VER
#pragma warning( disable : 4702 ) // warning C4702: unreachable code
#endif
int SocketLayer::SendTo( SOCKET s, const char *data, int length, unsigned int binaryAddress, unsigned short port )
{
	if ( s == (SOCKET) -1 )
	{
		return -1;
	}

	int len;

#if defined(_PS3) && defined (_PS3_LOBBY)
	sockaddr_in_p2p sa;
	memset(&sa, 0, sizeof(sa));
	// LAME!!!! You have to know the behind-nat port on the recipient! Just guessing it is the same as our own
	sa.sin_vport = htons(HACK_APP_PORT);
	sa.sin_port = htons(port); // Port returned from signaling
#else
	sockaddr_in sa;
	sa.sin_port = htons( port ); // User port
#endif

	sa.sin_addr.s_addr = binaryAddress;
	sa.sin_family = AF_INET;

#if 1 // !defined(_WIN32)
	do
	{
		len = sendto( s, data, length, 0, ( const sockaddr* ) & sa, sizeof( sa ) );
	}
	while ( len == 0 );
#else
	WSABUF DataBuf;
	DataBuf.len = length;
	DataBuf.buf = (char*) data;
	DWORD bytesSent;
	int WSASendToResult = WSASendTo(s,
		&DataBuf,
		1,
		&bytesSent,
		0,
		(SOCKADDR*) &sa,
		 sizeof( sa ),
		0,
		NULL);
	len=bytesSent;
	if (WSASendToResult!=0)
	{
		DWORD dwIOError = WSAGetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "sendto failed:Error code - %d\n%s", dwIOError, messageBuffer );

		//Free the buffer.
		LocalFree( messageBuffer );
	}
#endif

	if ( len != -1 )
		return 0;

#if defined(_WIN32)

	DWORD dwIOError = WSAGetLastError();

	if ( dwIOError == WSAECONNRESET )
	{
#if defined(_DEBUG)
		printf( "A previous send operation resulted in an ICMP Port Unreachable message.\n" );
#endif

	}
	else if ( dwIOError != WSAEWOULDBLOCK )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "sendto failed:Error code - %d\n%s", dwIOError, messageBuffer );

		//Free the buffer.
		LocalFree( messageBuffer );
#endif

	}

	return dwIOError;
#endif

	return 1; // error
}

int SocketLayer::SendTo( SOCKET s, const char *data, int length, const char ip[ 16 ], unsigned short port )
{
	unsigned int binaryAddress;
	binaryAddress = inet_addr( ip );
	return SendTo( s, data, length, binaryAddress, port );
}
int SocketLayer::SendToTTL( SOCKET s, const char *data, int length, const char ip[ 16 ], unsigned short port, int ttl )
{
#if !defined(_XBOX360)
	int oldTTL;
	socklen_t opLen=sizeof(oldTTL);
	// Get the current TTL
	if (getsockopt(s, IPPROTO_IP, IP_TTL, ( char * ) & oldTTL, &opLen ) == -1)
	{
#if defined(_WIN32) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "getsockopt(IPPROTO_IP,IP_TTL) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif
	}

	// Set to TTL
	int newTTL=ttl;
	if (setsockopt(s, IPPROTO_IP, IP_TTL, ( char * ) & newTTL, sizeof ( newTTL ) ) == -1)
	{

#if defined(_WIN32) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "setsockopt(IPPROTO_IP,IP_TTL) failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
#endif
	}

	// Send
	int res = SendTo(s,data,length,ip,port);

	// Restore the old TTL
	setsockopt(s, IPPROTO_IP, IP_TTL, ( char * ) & oldTTL, opLen );

	return res;
#else
	return 0;
#endif
}

#if !defined(_XBOX360)
void SocketLayer::GetMyIP( char ipList[ 10 ][ 16 ] )
{
#if !defined(_PS3)
	char ac[ 80 ];
	if ( gethostname( ac, sizeof( ac ) ) == -1 )
	{
	#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "gethostname failed:Error code - %d\n%s", dwIOError, messageBuffer );
		//Free the buffer.
		LocalFree( messageBuffer );
	#endif

		return ;
	}

	struct hostent *phe = gethostbyname( ac );


	if ( phe == 0 )
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "gethostbyname failed:Error code - %d\n%s", dwIOError, messageBuffer );

		//Free the buffer.
		LocalFree( messageBuffer );
#endif

		return ;
	}

	for ( int i = 0; phe->h_addr_list[ i ] != 0 && i < 10; ++i )
	{

		struct in_addr addr;

		memcpy( &addr, phe->h_addr_list[ i ], sizeof( struct in_addr ) );
		//cout << "Address " << i << ": " << inet_ntoa(addr) << endl;
		strcpy( ipList[ i ], inet_ntoa( addr ) );
	}
#else
	union CellNetCtlInfo info;
	int errCode;
	if((errCode=cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &info)) >= 0){
		memcpy(ipList[0], info.ip_address, sizeof(info.ip_address));
		ipList[1][0]=0;
	}
	return;
#endif

}
#endif

unsigned short SocketLayer::GetLocalPort ( SOCKET s )
{
	sockaddr_in sa;
	socklen_t len = sizeof(sa);
	if (getsockname(s, (sockaddr*)&sa, &len)!=0)
	{
#if defined(_WIN32) && !defined(_XBOX360) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPVOID messageBuffer;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
			( LPTSTR ) & messageBuffer, 0, NULL );
		// something has gone wrong here...
		printf( "getsockname failed:Error code - %d\n%s", dwIOError, messageBuffer );

		//Free the buffer.
		LocalFree( messageBuffer );
#endif
		return 0;
	}
	return ntohs(sa.sin_port);
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
