#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "Game.h"
#include "Logger.h"
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

class Sd0Test : public ::testing::Test {
protected:
	void SetUp() override {
		Game::logger = new Logger("./testing.log", true, true);
	}

	void TearDown() override {
		delete Game::logger;
		Game::logger = nullptr;
	}

	// One zlib chunk as it sits in an sd0 buffer: its size and then its data
	static std::string MakeChunk(const std::string& data) {
		std::vector<uint8_t> compressed(ZCompression::GetMaxCompressedLength(data.size()));
		const auto compressedSize = ZCompression::Compress(
			reinterpret_cast<const uint8_t*>(data.data()), data.size(),
			compressed.data(), compressed.size());
		EXPECT_NE(compressedSize, -1);

		std::string chunk;
		const uint32_t size = compressedSize;
		chunk.append(reinterpret_cast<const char*>(&size), sizeof(size));
		chunk.append(reinterpret_cast<const char*>(compressed.data()), size);
		return chunk;
	}

	static std::string Inflate(const std::string& sd0Data) {
		std::istringstream stream(sd0Data);
		const Sd0 sd0(stream);
		return sd0.GetAsStringUncompressed();
	}

	const std::string m_Header = std::string(Sd0::SD0_HEADER, 5);
};

TEST_F(Sd0Test, DataOfSeveralChunksSurvivesARoundTrip) {
	std::string data;
	for (uint32_t i = 0; data.size() < Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE * 2 + 1000; i++) data += std::to_string(i) + ",";

	std::istringstream empty;
	Sd0 sd0(empty);
	sd0.FromData(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	EXPECT_EQ(sd0.GetAsVector().size(), 3);
	EXPECT_EQ(sd0.GetAsStringUncompressed(), data);

	EXPECT_EQ(Inflate(sd0.GetAsStream().str()), data);
}

TEST_F(Sd0Test, ChunksThatAreNotFullFollowOneAnother) {
	EXPECT_EQ(Inflate(m_Header + MakeChunk("abc") + MakeChunk("def") + MakeChunk("ghi")), "abcdefghi");
}

TEST_F(Sd0Test, TooManyChunksAreRefused) {
	const auto chunk = MakeChunk("a");

	std::string allowed = m_Header;
	for (uint32_t i = 0; i < Sd0::MAX_CHUNKS; i++) allowed += chunk;
	EXPECT_EQ(Inflate(allowed), std::string(Sd0::MAX_CHUNKS, 'a'));

	EXPECT_EQ(Inflate(allowed + chunk), "");
}

TEST_F(Sd0Test, TooMuchInflatedDataIsRefused) {
	const auto chunk = MakeChunk(std::string(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE, 'a'));
	const uint32_t chunksToFill = Sd0::MAX_UNCOMPRESSED_SIZE / Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE;

	std::string allowed = m_Header;
	for (uint32_t i = 0; i < chunksToFill; i++) allowed += chunk;
	EXPECT_EQ(Inflate(allowed).size(), Sd0::MAX_UNCOMPRESSED_SIZE);

	EXPECT_EQ(Inflate(allowed + MakeChunk("a")), "");
}
