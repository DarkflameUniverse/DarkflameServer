/// \file
/// \brief This will write all incoming and outgoing network messages to a file
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

#ifndef __PACKET_FILE_LOGGER_H_
#define __PACKET_FILE_LOGGER_H_

#include "PacketLogger.h"
#include <stdio.h>

/// \ingroup PACKETLOGGER_GROUP
/// \brief Packetlogger that outputs to a file
class RAK_DLL_EXPORT  PacketFileLogger : public PacketLogger
{
public:
	PacketFileLogger();
	virtual ~PacketFileLogger();
	void StartLog(const char *filenamePrefix);
	virtual void WriteLog(const char *str);
protected:
	FILE *packetLogFile;
};

#endif
