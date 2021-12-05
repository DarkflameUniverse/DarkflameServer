/// \file
/// \brief DataCompressor does compression on a block of data.  Not very good compression, but it's small and fast so is something you can use per-message at runtime.
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


#ifndef __DATA_COMPRESSOR_H
#define __DATA_COMPRESSOR_H

#include "RakMemoryOverride.h"
#include "DS_HuffmanEncodingTree.h"
#include "Export.h"

/// \brief Does compression on a block of data.  Not very good compression, but it's small and fast so is something you can compute at runtime.
class RAK_DLL_EXPORT DataCompressor : public RakNet::RakMemoryOverride
{
public:
	static void Compress( unsigned char *userData, unsigned sizeInBytes, RakNet::BitStream * output );
	static unsigned DecompressAndAllocate( RakNet::BitStream * input, unsigned char **output );
};

#endif
