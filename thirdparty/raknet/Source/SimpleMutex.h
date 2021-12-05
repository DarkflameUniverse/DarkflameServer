/// \file
/// \brief \b [Internal] Encapsulates a mutex
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

#ifndef __SIMPLE_MUTEX_H
#define __SIMPLE_MUTEX_H

#include "RakMemoryOverride.h"
#ifdef _XBOX360
#include "Console1Includes.h"
#elif defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <sys/types.h>
#endif
#include "Export.h"
/// \brief An easy to use mutex.
/// 
/// I wrote this because the version that comes with Windows is too complicated and requires too much code to use.
/// @remark Previously I used this everywhere, and in fact for a year or two RakNet was totally threadsafe.  While doing profiling, I saw that this function was incredibly slow compared to the blazing performance of everything else, so switched to single producer / consumer everywhere.  Now the user thread of RakNet is not threadsafe, but it's 100X faster than before.
class RAK_DLL_EXPORT SimpleMutex : public RakNet::RakMemoryOverride
{
public:

	/// Constructor
	SimpleMutex();
	
	// Destructor
	~SimpleMutex();
	
	// Locks the mutex.  Slow!
	void Lock(void);
	
	// Unlocks the mutex.
	void Unlock(void);
private:
	#ifdef _WIN32
	CRITICAL_SECTION criticalSection; /// Docs say this is faster than a mutex for single process access
	#else
	pthread_mutex_t hMutex;
	#endif
};

#endif

