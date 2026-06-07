#pragma once

#include <cstdint>

namespace ZCompression {
	uint32_t GetMaxCompressedLength(uint32_t nLenSrc);

	int32_t Compress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst);

	int32_t Decompress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst, int32_t& nErr);
}

