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

// No longer used as I no longer support IO Completion ports
/*
#ifdef __USE_IO_COMPLETION_PORTS
#include "ExtendedOverlappedPool.h"

ExtendedOverlappedPool ExtendedOverlappedPool::I;

ExtendedOverlappedPool::ExtendedOverlappedPool()
{}

ExtendedOverlappedPool::~ExtendedOverlappedPool()
{
	// The caller better have returned all the packets!
	ExtendedOverlappedStruct * p;
	poolMutex.Lock();

	while ( pool.Size() )
	{
		p = pool.Pop();
		delete p;
	}

	poolMutex.Unlock();
}

ExtendedOverlappedStruct* ExtendedOverlappedPool::GetPointer( void )
{
	ExtendedOverlappedStruct * p = 0;
	poolMutex.Lock();

	if ( pool.Size() )
		p = pool.Pop();

	poolMutex.Unlock();

	if ( p )
		return p;

	return new ExtendedOverlappedStruct;
}

void ExtendedOverlappedPool::ReleasePointer( ExtendedOverlappedStruct *p )
{
	poolMutex.Lock();
	pool.Push( p );
	poolMutex.Unlock();
}

#endif

*/
