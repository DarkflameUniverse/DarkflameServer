#include "ZCompression.h"

#ifdef DARKFLAME_PLATFORM_WIN32
#include "zlib-ng.h"

// Yes, I know there is a "zlib compat mode", it doesn't work, it's really fucking dumb

#define z_stream zng_stream

#define z_deflateInit zng_deflateInit
#define z_deflate zng_deflate
#define z_deflateEnd zng_deflateEnd

#define z_inflateInit zng_inflateInit
#define z_inflate zng_inflate
#define z_inflateEnd zng_inflateEnd


#else
#include "zlib.h"
#endif

namespace ZCompression {
	int32_t GetMaxCompressedLength(int32_t nLenSrc) {
		int32_t n16kBlocks = (nLenSrc + 16383) / 16384; // round up any fraction of a block
		return (nLenSrc + 6 + (n16kBlocks * 5));
	}

	int32_t Compress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, int32_t nLenDst) {
        
		z_stream zInfo = { 0 };
		zInfo.total_in = zInfo.avail_in = nLenSrc;
		zInfo.total_out = zInfo.avail_out = nLenDst;
		zInfo.next_in = const_cast<Bytef*>(abSrc);
		zInfo.next_out = abDst;

		int nErr, nRet = -1;
		nErr = z_deflateInit(&zInfo, Z_DEFAULT_COMPRESSION); // zlib function
		if (nErr == Z_OK) {
			nErr = z_deflate(&zInfo, Z_FINISH);              // zlib function
			if (nErr == Z_STREAM_END) {
				nRet = zInfo.total_out;
			}
		}
		z_deflateEnd(&zInfo);    // zlib function
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
		nErr = z_inflateInit(&zInfo); // zlib function
		if (nErr == Z_OK) {
			nErr = z_inflate(&zInfo, Z_FINISH); // zlib function
			if (nErr == Z_STREAM_END) {
				nRet = zInfo.total_out;
			}
		}
		z_inflateEnd(&zInfo); // zlib function
		return(nRet);
	}
}

