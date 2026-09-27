#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ZCompression {
	uint32_t GetMaxCompressedLength(uint32_t nLenSrc);

	int32_t Compress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst);

	int32_t Decompress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst, int32_t& nErr);

	// `data` as a gzip file (RFC 1952), what HTTP clients and the game client's UGC downloads inflate
	std::string Gzip(std::string_view data);

	// A gzip file's contents; nullopt when it isn't one or is damaged
	std::optional<std::string> Gunzip(std::string_view data);

	// Raw deflate data (no zlib or gzip header, as in zip files) that inflates to `size` bytes; nullopt when it doesn't
	std::optional<std::string> InflateRaw(std::string_view data, size_t size);
}

