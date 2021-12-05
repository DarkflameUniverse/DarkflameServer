#include "RakMemoryOverride.h"
#include "RakAssert.h"
#include <stdlib.h>

#if defined (_PS3)
#include "Console2Includes.h"
#endif

using namespace RakNet;

void DefaultOutOfMemoryHandler(const char *file, const long line)
{
	(void) file;
	(void) line;
	RakAssert(0);
}

void* (*rakMalloc) (size_t size) = RakMemoryOverride::RakMalloc;
void* (*rakRealloc) (void *p, size_t size) = RakMemoryOverride::RakRealloc;
void (*rakFree) (void *p) = RakMemoryOverride::RakFree;
void (*notifyOutOfMemory) (const char *file, const long line)=DefaultOutOfMemoryHandler;

#ifdef _USE_RAK_MEMORY_OVERRIDE
void* RakMemoryOverride::operator new (size_t size)
{
	return rakMalloc(size); 
}
void RakMemoryOverride::operator delete (void *p)
{
	return rakFree(p);
}
void* RakMemoryOverride::operator new[] (size_t size)
{ 
	return rakMalloc(size); 
}
void RakMemoryOverride::operator delete[] (void *p)
{
	return rakFree(p);
}
#endif

void* RakMemoryOverride::RakMalloc (size_t size)
{
	return malloc(size);
}

void* RakMemoryOverride::RakRealloc (void *p, size_t size)
{
	return realloc(p,size);
}

void RakMemoryOverride::RakFree (void *p)
{
	free(p);
}
