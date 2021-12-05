/// \file
/// \brief Derivation of the packet logger to defer the call to WriteLog until the user thread.
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

#ifndef __THREADSAFE_PACKET_LOGGER_H
#define __THREADSAFE_PACKET_LOGGER_H

#include "PacketLogger.h"
#include "SingleProducerConsumer.h"

/// \ingroup PACKETLOGGER_GROUP
/// \brief Same as PacketLogger, but writes output in the user thread.
class RAK_DLL_EXPORT ThreadsafePacketLogger : public PacketLogger
{
public:
	ThreadsafePacketLogger();
	virtual ~ThreadsafePacketLogger();

	virtual void Update(RakPeerInterface *peer);

protected:
	virtual void AddToLog(const char *str);

	DataStructures::SingleProducerConsumer<char*> logMessages;
};

#endif
