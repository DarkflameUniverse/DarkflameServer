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

#include "SimpleMutex.h"
#include <assert.h>

SimpleMutex::SimpleMutex()
{
#ifdef _WIN32
	//	hMutex = CreateMutex(NULL, FALSE, 0);
	//	assert(hMutex);
	InitializeCriticalSection(&criticalSection);
#else
	int error = pthread_mutex_init(&hMutex, 0);
	(void) error;
	assert(error==0);
#endif
}

SimpleMutex::~SimpleMutex()
{
#ifdef _WIN32
	//	CloseHandle(hMutex);
	DeleteCriticalSection(&criticalSection);
#else
	pthread_mutex_destroy(&hMutex);
#endif
}

#ifdef _WIN32
#ifdef _DEBUG
#include <stdio.h>
#endif
#endif

void SimpleMutex::Lock(void)
{
#ifdef _WIN32
	/*
	DWORD d = WaitForSingleObject(hMutex, INFINITE);
	#ifdef _DEBUG
	if (d==WAIT_FAILED)
	{
	LPVOID messageBuffer;
	FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER | 
	FORMAT_MESSAGE_FROM_SYSTEM | 
	FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL,
	GetLastError(),
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	(LPTSTR) &messageBuffer,
	0,
	NULL 
	);
	// Process any inserts in messageBuffer.
	// ...
	// Display the string.
	//MessageBox( NULL, (LPCTSTR)messageBuffer, "Error", MB_OK | MB_ICONINFORMATION );
	printf("SimpleMutex error: %s", messageBuffer);
	// Free the buffer.
	LocalFree( messageBuffer );

	}

	assert(d==WAIT_OBJECT_0);
	*/
	EnterCriticalSection(&criticalSection);

#else
	int error = pthread_mutex_lock(&hMutex);
	(void) error;
	assert(error==0);
#endif
}

void SimpleMutex::Unlock(void)
{
#ifdef _WIN32
	//	ReleaseMutex(hMutex);
	LeaveCriticalSection(&criticalSection);
#else
	int error = pthread_mutex_unlock(&hMutex);
	(void) error;
	assert(error==0);
#endif
}

