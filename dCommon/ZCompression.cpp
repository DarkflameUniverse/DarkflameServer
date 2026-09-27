#include "ZCompression.h"

#include <algorithm>

#include "zlib.h"

namespace ZCompression {
	uint32_t GetMaxCompressedLength(uint32_t nLenSrc) {
		uint32_t n16kBlocks = (nLenSrc + 16383) / 16384; // round up any fraction of a block
		return (nLenSrc + 6 + (n16kBlocks * 5));
	}

	int32_t Compress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst) {
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

	int32_t Decompress(const uint8_t* abSrc, uint32_t nLenSrc, uint8_t* abDst, uint32_t nLenDst, int32_t& nErr) {
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

	namespace {
		// Inflate everything in `data` with zlib's `windowBits` (31: gzip, -15: raw); nullopt on any error
		std::optional<std::string> Inflate(std::string_view data, int windowBits, size_t sizeHint) {
			z_stream stream{};
			if (inflateInit2(&stream, windowBits) != Z_OK) return std::nullopt;
			std::string out;
			out.reserve(sizeHint);
			stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
			stream.avail_in = static_cast<uInt>(data.size());
			char buffer[65536];
			int result = Z_OK;
			while (result == Z_OK) {
				stream.next_out = reinterpret_cast<Bytef*>(buffer);
				stream.avail_out = sizeof(buffer);
				result = inflate(&stream, Z_NO_FLUSH);
				if (result != Z_OK && result != Z_STREAM_END) break;
				out.append(buffer, sizeof(buffer) - stream.avail_out);
				if (result == Z_OK && stream.avail_in == 0 && stream.avail_out != 0) break; // truncated
			}
			inflateEnd(&stream);
			if (result != Z_STREAM_END) return std::nullopt;
			return out;
		}
	}

	std::string Gzip(std::string_view data) {
		z_stream stream{};
		// 31: a gzip header and trailer instead of zlib's
		if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};
		std::string out;
		out.resize(deflateBound(&stream, static_cast<uLong>(data.size())) + 32);
		stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
		stream.avail_in = static_cast<uInt>(data.size());
		stream.next_out = reinterpret_cast<Bytef*>(out.data());
		stream.avail_out = static_cast<uInt>(out.size());
		const auto result = deflate(&stream, Z_FINISH);
		out.resize(stream.total_out);
		deflateEnd(&stream);
		return result == Z_STREAM_END ? out : std::string{};
	}

	std::optional<std::string> Gunzip(std::string_view data) {
		return Inflate(data, 31, data.size() * 4);
	}

	std::optional<std::string> InflateRaw(std::string_view data, size_t size) {
		auto out = Inflate(data, -15, size);
		if (!out || out->size() != size) return std::nullopt;
		return out;
	}

	uint32_t Crc32(uint32_t crc, std::string_view data) {
		while (!data.empty()) {
			const auto piece = static_cast<uInt>(std::min<size_t>(data.size(), 1u << 30));
			crc = static_cast<uint32_t>(crc32(crc, reinterpret_cast<const Bytef*>(data.data()), piece));
			data.remove_prefix(piece);
		}
		return crc;
	}

	struct RawDeflater::State {
		z_stream stream{};
		bool ok{};
	};

	RawDeflater::RawDeflater(Sink sink, int level) : m_State(std::make_unique<State>()), m_Sink(std::move(sink)) {
		m_State->ok = deflateInit2(&m_State->stream, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK;
	}

	RawDeflater::~RawDeflater() {
		deflateEnd(&m_State->stream);
	}

	bool RawDeflater::Run(std::string_view data, int flush) {
		if (!m_State->ok) return false;
		auto& stream = m_State->stream;
		stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
		stream.avail_in = static_cast<uInt>(data.size());
		char buffer[64 * 1024];
		int result = Z_OK;
		do {
			stream.next_out = reinterpret_cast<Bytef*>(buffer);
			stream.avail_out = sizeof(buffer);
			result = deflate(&stream, flush);
			if (result == Z_STREAM_ERROR) return m_State->ok = false;
			const size_t made = sizeof(buffer) - stream.avail_out;
			m_Out += made;
			if (made && !m_Sink(std::string_view(buffer, made))) return m_State->ok = false;
		} while (stream.avail_out == 0 || (flush == Z_FINISH && result != Z_STREAM_END));
		return true;
	}

	bool RawDeflater::Write(std::string_view data) {
		m_In += data.size();
		while (data.size() > (1u << 30)) {
			if (!Run(data.substr(0, 1u << 30), Z_NO_FLUSH)) return false;
			data.remove_prefix(1u << 30);
		}
		return Run(data, Z_NO_FLUSH);
	}

	bool RawDeflater::Finish() {
		const bool ok = Run({}, Z_FINISH);
		m_State->ok = false;
		return ok;
	}
}
