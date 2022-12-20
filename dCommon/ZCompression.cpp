#include "ZCompression.h"

#include <zlib.h>

namespace ZCompression {
	int32_t GetMaxCompressedLength(int32_t nLenSrc) {
		int32_t n16kBlocks = (nLenSrc + 16383) / 16384; // round up any fraction of a block
		return (nLenSrc + 6 + (n16kBlocks * 5));
	}

	int32_t GZipCompress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst) {
		z_stream stream;
		stream.next_in = (Bytef*)abSrc;
		stream.avail_in = nLenSrc;
		stream.next_out = (Bytef*)abDst;
		stream.avail_out = nLenDst;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;

		int32_t nErr, nRet = -1;
		nErr = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
		if (nErr == Z_OK) {
			nErr = deflate(&stream, Z_FINISH);
			if (nErr == Z_STREAM_END) {
				nRet = stream.total_out;
			} else {
				deflateEnd(&stream);
				return nErr;
			}
		} else {
			return nErr;
		}

		nErr = deflateEnd(&stream);
		return (nErr == Z_OK) ? nRet : nErr;
	}

	int32_t Compress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst) {
		z_stream zInfo = { 0 };
		zInfo.total_in = zInfo.avail_in = nLenSrc;
		zInfo.total_out = zInfo.avail_out = nLenDst;
		zInfo.next_in = const_cast<Bytef*>(abSrc);
		zInfo.next_out = abDst;

		int nErr, nRet = -1;
		nErr = deflateInit(&zInfo, Z_DEFAULT_COMPRESSION); // zlib function
		if (nErr == Z_OK) {
			nErr = deflate(&zInfo, Z_FINISH);              // zlib function
			if (nErr == Z_STREAM_END) {
				nRet = zInfo.total_out;
			}
		}
		deflateEnd(&zInfo);    // zlib function
		return(nRet);
	}

	int32_t Decompress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst, int32_t& nErr) {
		// Get the size of the decompressed data
		z_stream zInfo = { 0 };
		zInfo.total_in = zInfo.avail_in = nLenSrc;
		zInfo.total_out = zInfo.avail_out = nLenDst;
		zInfo.next_in = const_cast<Bytef*>(abSrc);
		zInfo.next_out = abDst;

		int nRet = -1;
		nErr = inflateInit(&zInfo); // zlib function
		if (nErr == Z_OK) {
			nErr = inflate(&zInfo, Z_FINISH); // zlib function
			if (nErr == Z_STREAM_END) {
				nRet = zInfo.total_out;
			}
		}
		inflateEnd(&zInfo); // zlib function
		return(nRet);
	}
}

