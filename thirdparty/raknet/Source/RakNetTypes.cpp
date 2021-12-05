/// \file
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

#include "RakNetTypes.h"
#include <string.h>
#include <stdio.h>

#ifdef _XBOX360
#include "Console1Includes.h"
#elif defined(_WIN32)
// IP_DONTFRAGMENT is different between winsock 1 and winsock 2.  Therefore, Winsock2.h must be linked againt Ws2_32.lib
// winsock.h must be linked against WSock32.lib.  If these two are mixed up the flag won't work correctly
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <string.h> // strncasecmp
#include "Itoa.h"
#include "SocketLayer.h"
#include <stdlib.h>

SocketDescriptor::SocketDescriptor() {port=0; hostAddress[0]=0;}
SocketDescriptor::SocketDescriptor(unsigned short _port, const char *_hostAddress)
{
	port=_port;
	if (_hostAddress)
		strcpy(hostAddress, _hostAddress);
	else
		hostAddress[0]=0;
}

// Defaults to not in peer to peer mode for NetworkIDs.  This only sends the localSystemAddress portion in the BitStream class
// This is what you want for client/server, where the server assigns all NetworkIDs and it is unnecessary to transmit the full structure.
// For peer to peer, this will transmit the systemAddress of the system that created the object in addition to localSystemAddress.  This allows
// Any system to create unique ids locally.
// All systems must use the same value for this variable.
bool RAK_DLL_EXPORT NetworkID::peerToPeerMode=false;

bool SystemAddress::operator==( const SystemAddress& right ) const
{
	return binaryAddress == right.binaryAddress && port == right.port;
}

bool SystemAddress::operator!=( const SystemAddress& right ) const
{
	return binaryAddress != right.binaryAddress || port != right.port;
}

bool SystemAddress::operator>( const SystemAddress& right ) const
{
	return ( ( binaryAddress > right.binaryAddress ) || ( ( binaryAddress == right.binaryAddress ) && ( port > right.port ) ) );
}

bool SystemAddress::operator<( const SystemAddress& right ) const
{
	return ( ( binaryAddress < right.binaryAddress ) || ( ( binaryAddress == right.binaryAddress ) && ( port < right.port ) ) );
}
const char *SystemAddress::ToString(bool writePort) const
{
	if (*this==UNASSIGNED_SYSTEM_ADDRESS)
		return "UNASSIGNED_SYSTEM_ADDRESS";

#ifdef _XBOX360
	return "";
#else
	static unsigned char strIndex=0;
	static char str[8][22];
	in_addr in;
	in.s_addr = binaryAddress;
	strcpy(str[strIndex], inet_ntoa( in ));
	if (writePort)
	{
		strcat(str[strIndex], ":");
		Itoa(port, str[strIndex]+strlen(str[strIndex]), 10);
	}

	unsigned char lastStrIndex=strIndex;
	strIndex++;
	if (strIndex==8)
		strIndex=0;
	
	return (char*) str[lastStrIndex];
#endif
}
#ifdef _MSC_VER
#pragma warning( disable : 4996 )  // The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _strnicmp. See online help for details.
#endif
void SystemAddress::SetBinaryAddress(const char *str)
{
	if (str[0]<'0' || str[0]>'9')
	{
#if !defined(_XBOX360)
		const char *ip = ( char* ) SocketLayer::Instance()->DomainNameToIP( str );
#endif
		if (ip)
		{
			binaryAddress=inet_addr(ip);
		}		
	}
	else
	{
		//#ifdef _XBOX360
		//	binaryAddress=UNASSIGNED_SYSTEM_ADDRESS.binaryAddress;
		//#else
		// Split the string into the first part, and the : part
		int index, portIndex;
		char IPPart[22];
		char portPart[10];
		// Only write the valid parts, don't change existing if invalid
		//	binaryAddress=UNASSIGNED_SYSTEM_ADDRESS.binaryAddress;
		//	port=UNASSIGNED_SYSTEM_ADDRESS.port;
		for (index=0; str[index] && str[index]!=':' && index<22; index++)
		{
			IPPart[index]=str[index];
		}
		IPPart[index]=0;
		portPart[0]=0;
		if (str[index] && str[index+1])
		{
			index++;
			for (portIndex=0; portIndex<10 && str[index] && index < 22+10; index++, portIndex++)
				portPart[portIndex]=str[index];
			portPart[portIndex]=0;
		}

#if defined(_WIN32)
		if (_strnicmp(str,"localhost", 9)==0)
#else
		if (strncasecmp(str,"localhost", 9)==0)
#endif
			binaryAddress=inet_addr("127.0.0.1");
		else if (IPPart[0])
			binaryAddress=inet_addr(IPPart);
		if (portPart[0])
			port=(unsigned short) atoi(portPart);
		//#endif
	}

}

NetworkID& NetworkID::operator = ( const NetworkID& input )
{
	systemAddress = input.systemAddress;
	localSystemAddress = input.localSystemAddress;
	return *this;
}

bool NetworkID::operator==( const NetworkID& right ) const
{
	if (NetworkID::peerToPeerMode)
		return systemAddress == right.systemAddress && localSystemAddress == right.localSystemAddress;
	else
		return localSystemAddress==right.localSystemAddress;
}

bool NetworkID::operator!=( const NetworkID& right ) const
{
	if (NetworkID::peerToPeerMode)
		return systemAddress != right.systemAddress || localSystemAddress != right.localSystemAddress;
	else
		return localSystemAddress!=right.localSystemAddress;
}

bool NetworkID::operator>( const NetworkID& right ) const
{
	if (NetworkID::peerToPeerMode)
		return ( ( systemAddress > right.systemAddress ) || ( ( systemAddress == right.systemAddress ) && ( localSystemAddress > right.localSystemAddress ) ) );
	else
		return localSystemAddress>right.localSystemAddress;
}

bool NetworkID::operator<( const NetworkID& right ) const
{
	if (NetworkID::peerToPeerMode)
		return ( ( systemAddress < right.systemAddress ) || ( ( systemAddress == right.systemAddress ) && ( localSystemAddress < right.localSystemAddress ) ) );
	else
		return localSystemAddress<right.localSystemAddress;
}

bool NetworkID::IsPeerToPeerMode(void)
{
	return peerToPeerMode;
}
void NetworkID::SetPeerToPeerMode(bool isPeerToPeer)
{
	peerToPeerMode=isPeerToPeer;
}
