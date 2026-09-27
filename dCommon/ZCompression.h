#pragma once

#include <cstdint>
#include <functional>
#include <memory>
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

	// CRC-32 (as in zip and gzip files) of `data`, continuing from `crc` (0 to start)
	uint32_t Crc32(uint32_t crc, std::string_view data);

	/**
	 * Raw deflate (as in zip files) of data given a piece at a time, so a big file never has to sit in memory: each
	 * Write and Finish hands what's compressed so far to `sink`.
	 */
	class RawDeflater {
	public:
		using Sink = std::function<bool(std::string_view)>; // false stops (e.g. the disk is full)
		explicit RawDeflater(Sink sink, int level = 6);
		~RawDeflater();
		RawDeflater(const RawDeflater&) = delete;
		RawDeflater& operator=(const RawDeflater&) = delete;

		bool Write(std::string_view data);
		// The rest of the compressed data; the deflater can't be written to afterwards
		bool Finish();
		uint64_t BytesIn() const { return m_In; }
		uint64_t BytesOut() const { return m_Out; }

	private:
		bool Run(std::string_view data, int flush);
		struct State;
		std::unique_ptr<State> m_State;
		Sink m_Sink;
		uint64_t m_In{};
		uint64_t m_Out{};
	};
}
