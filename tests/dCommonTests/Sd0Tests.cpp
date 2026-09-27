#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <sstream>
#include <string>

#include "Sd0.h"
#include "ZCompression.h"

namespace {
	// The chunks as the 1.10.64 client reads them (Sd0Decompress): the header, then a u32 size and zlib data per chunk,
	// each inflating to at most 256 KiB
	std::string InflateLikeTheClient(const std::string& sd0, size_t& chunks) {
		EXPECT_TRUE(sd0.starts_with(std::string(Sd0::SD0_HEADER, 5)));
		size_t offset = 5;
		std::string inflated;
		chunks = 0;
		while (offset < sd0.size()) {
			if (offset + 4 > sd0.size()) return {};
			uint32_t length{};
			std::memcpy(&length, sd0.data() + offset, 4);
			offset += 4;
			if (offset + length > sd0.size()) return {};
			std::string chunk(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE, '\0');
			int32_t error{};
			const auto size = ZCompression::Decompress(reinterpret_cast<const uint8_t*>(sd0.data() + offset), length,
				reinterpret_cast<uint8_t*>(chunk.data()), static_cast<uint32_t>(chunk.size()), error);
			if (size < 0) return {};
			inflated.append(chunk.data(), static_cast<size_t>(size));
			offset += length;
			chunks++;
		}
		return inflated;
	}
}

TEST(Sd0, CompressesInChunksTheClientReads) {
	// 600 KiB: chunks of 256, 256 and 88 KiB
	std::string data(600 * 1024, '\0');
	for (size_t i = 0; i < data.size(); i++) data[i] = static_cast<char>((i * 7) % 251);
	const auto sd0 = Sd0::Compress(data);
	size_t chunks{};
	EXPECT_EQ(InflateLikeTheClient(sd0, chunks), data);
	EXPECT_EQ(chunks, 3u);

	// And back through the reader
	std::istringstream stream(sd0);
	EXPECT_EQ(Sd0(stream).GetAsStringUncompressed(), data);
}

TEST(Sd0, CompressesDataThatDoesNotShrink) {
	// Random bytes grow a little when deflated: a full chunk must still fit
	std::string data(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE + 1000, '\0');
	std::mt19937 random(1234);
	for (auto& c : data) c = static_cast<char>(random());
	const auto sd0 = Sd0::Compress(data);
	size_t chunks{};
	EXPECT_EQ(InflateLikeTheClient(sd0, chunks), data);
	EXPECT_EQ(chunks, 2u);
}

TEST(Sd0, EmptyDataIsTheHeaderOnly) {
	EXPECT_EQ(Sd0::Compress(""), std::string(Sd0::SD0_HEADER, 5));
	Sd0 empty;
	EXPECT_TRUE(empty.GetAsVector().empty());
}
