#include "ThreadsafePacketLogger.h"
#include <string.h>

ThreadsafePacketLogger::ThreadsafePacketLogger()
{

}
ThreadsafePacketLogger::~ThreadsafePacketLogger()
{
	char **msg;
	while ((msg = logMessages.ReadLock()) != 0)
	{
		rakFree((*msg));
	}
}
void ThreadsafePacketLogger::Update(RakPeerInterface *peer)
{
	(void) peer;

	char **msg;
	while ((msg = logMessages.ReadLock()) != 0)
	{
		WriteLog(*msg);
		rakFree((*msg));
	}
}
void ThreadsafePacketLogger::AddToLog(const char *str)
{
	char **msg = logMessages.WriteLock();
	*msg = (char*) rakMalloc( strlen(str)+1 );
	strcpy(*msg, str);
	logMessages.WriteUnlock();
}
