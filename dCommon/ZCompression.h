#pragma once

#include <cstdint>

#include "dPlatforms.h"

#ifndef DARKFLAME_PLATFORM_WIN32

namespace ZCompression
{
	int32_t GetMaxCompressedLength(int32_t nLenSrc);

	int32_t Compress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst);

	int32_t Decompress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst, int32_t& nErr);
}

#endif
