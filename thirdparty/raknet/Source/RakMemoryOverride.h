#ifndef __RAK_MEMORY_H
#define __RAK_MEMORY_H

#include "Export.h"
#include "RakNetDefines.h"

#ifdef _XBOX360
#elif defined (_PS3)
// Causes linker errors
// #include <stdlib.h>
typedef unsigned int size_t;
#elif defined ( __APPLE__ ) || defined ( __APPLE_CC__ )
#include <malloc/malloc.h>
#elif defined(_WIN32)
#include <malloc.h>
#else
#if !defined ( __FreeBSD__ )
#include <alloca.h>
#endif
#include <stdlib.h>
#endif

extern void* (*rakMalloc) (size_t size);
extern void* (*rakRealloc) (void *p, size_t size);
extern void (*rakFree) (void *p);
extern void (*notifyOutOfMemory) (const char *file, const long line);

namespace RakNet
{

class RAK_DLL_EXPORT RakMemoryOverride
{
public:
#ifdef _USE_RAK_MEMORY_OVERRIDE
	static void* operator new (size_t size);
	static void operator delete (void *p);
	static void* operator new[] (size_t size);
	static void operator delete[] (void *p);
#endif

	static void* RakMalloc (size_t size);
	static void* RakRealloc (void *p, size_t size);	
	static void RakFree (void *p);
	
};

}

#endif
