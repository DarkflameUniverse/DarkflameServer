/// \file
/// \brief \b [Internal] Depreciated, back from when I supported IO Completion ports.
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

#ifndef __CLIENT_CONTEXT_STRUCT_H
#define __CLIENT_CONTEXT_STRUCT_H

#ifdef _XBOX360
#elif defined(_WIN32)
//#include <windows.h>
#endif
#include "RakNetTypes.h"
#include "MTUSize.h"

class RakPeer;

#ifdef __USE_IO_COMPLETION_PORTS

struct ClientContextStruct
{
	HANDLE handle; // The socket, also used as a file handle
};

struct ExtendedOverlappedStruct
{
	OVERLAPPED overlapped;
	char data[ MAXIMUM_MTU_SIZE ]; // Used to hold data to send
	int length; // Length of the actual data to send, always under MAXIMUM_MTU_SIZE
	unsigned int binaryAddress;
	unsigned short port;
	RakPeer *rakPeer;
	bool read; // Set to true for reads, false for writes
};

#endif

#endif
