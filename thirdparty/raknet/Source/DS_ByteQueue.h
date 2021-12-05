/// \file
/// \brief \b [Internal] Byte queue
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

#ifndef __BYTE_QUEUE_H
#define __BYTE_QUEUE_H

#include "RakMemoryOverride.h"
#include "Export.h"

/// The namespace DataStructures was only added to avoid compiler errors for commonly named data structures
/// As these data structures are stand-alone, you can use them outside of RakNet for your own projects if you wish.
namespace DataStructures
{
	class ByteQueue : public RakNet::RakMemoryOverride
	{
	public:
		ByteQueue();
		~ByteQueue();
		void WriteBytes(const char *in, unsigned length);
		bool ReadBytes(char *out, unsigned length, bool peek);
		unsigned GetBytesWritten(void) const;
		void IncrementReadOffset(unsigned length);
		void Clear(void);
		void Print(void);

	protected:
		char *data;
		unsigned readOffset, writeOffset, lengthAllocated;
	};
}

#endif
