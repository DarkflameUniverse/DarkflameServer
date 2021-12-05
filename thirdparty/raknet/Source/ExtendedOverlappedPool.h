/// \file
/// \brief \b [Depreciated] This was used for IO completion ports.
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

// No longer used as I no longer support IO Completion ports
/*
#ifdef __USE_IO_COMPLETION_PORTS
#ifndef __EXTENDED_OVERLAPPED_POOL
#define __EXTENDED_OVERLAPPED_POOL
#include "SimpleMutex.h"
#include "ClientContextStruct.h"
#include "DS_Queue.h"

/// Depreciated - for IO completion ports
class ExtendedOverlappedPool
{

public:
	ExtendedOverlappedPool();
	~ExtendedOverlappedPool();
	ExtendedOverlappedStruct* GetPointer( void );
	void ReleasePointer( ExtendedOverlappedStruct *p );
	static inline ExtendedOverlappedPool* Instance()
	{
		return & I;
	}

private:
	DataStructures::Queue<ExtendedOverlappedStruct*> pool;
	SimpleMutex poolMutex;
	static ExtendedOverlappedPool I;
};

#endif
#endif

*/
