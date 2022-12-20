#pragma once

#include <cstdint>

namespace ZCompression {
	int32_t GetMaxCompressedLength(int32_t nLenSrc);

	int32_t GZipCompress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst);
	int32_t Compress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst);

	int32_t Decompress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst, int32_t& nErr);

	/**
	 * @brief Max size of an inflated sd0 zlib chunk
	 *
	 */
	constexpr uint32_t MAX_SD0_CHUNK_SIZE = 1024 * 256;
}

