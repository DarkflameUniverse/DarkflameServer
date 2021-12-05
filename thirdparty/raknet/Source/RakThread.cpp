#include "RakThread.h"
#include "RakNetDefines.h"

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
	#if !defined(_WIN32_WCE)
	#include <process.h>
	#endif
#else
#include <pthread.h>
#endif

using namespace RakNet;

#if defined(_WIN32_WCE)
int RakThread::Create( LPTHREAD_START_ROUTINE start_address, void *arglist)
#elif defined(_WIN32)
int RakThread::Create( unsigned __stdcall start_address( void* ), void *arglist)
#else
int RakThread::Create( void* start_address( void* ), void *arglist)
#endif
{
#ifdef _WIN32
	HANDLE threadHandle;
	unsigned threadID = 0;
#ifdef _XBOX360
	threadHandle = (HANDLE) _beginthreadex( NULL, 0, start_address, arglist, 0, &threadID );
#elif defined (_WIN32_WCE)
	threadHandle = CreateThread(NULL,MAX_ALLOCA_STACK_ALLOCATION*2,start_address,arglist,0,(DWORD*)&threadID);
#else
	threadHandle = (HANDLE) _beginthreadex( NULL, MAX_ALLOCA_STACK_ALLOCATION*2, start_address, arglist, 0, &threadID );
#endif

	if (threadHandle==0)
	{
		return 1;
	}
	else
	{
		CloseHandle( threadHandle );
		return 0;
	}
#else
	pthread_t threadHandle;
	// Create thread linux
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

	return pthread_create( &threadHandle, &attr, start_address, arglist );
#endif
}
