#include <gtest/gtest.h>

#include <cstring>
#include <sstream>

#include "Raw.h"
#include "TerrainMap.h"

namespace {
	// A version 32 terrain file (.raw) laid out as the client's are (little-endian)
	struct RawWriter {
		std::string data;
		template<typename T> RawWriter& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		RawWriter& Bytes(size_t count, uint8_t value) { data.append(count, static_cast<char>(value)); return *this; }

		// A size x size chunk at (x, z) with maps of `mapSize`, one flair and one scene per half of the scene map
		void Chunk(uint32_t id, uint32_t size, float x, float z, uint32_t mapSize, uint8_t sceneA, uint8_t sceneB, bool mesh) {
			Put(id).Put(size).Put(size).Put(x).Put(z);
			for (uint32_t texture : { 10u, 11u, 12u, 13u }) Put(texture);
			Put(2.0f);
			for (uint32_t i = 0; i < size * size; i++) Put(static_cast<float>(i));
			Put(mapSize).Bytes(static_cast<size_t>(mapSize) * mapSize * 4, 0x80); // color map
			Put<uint32_t>(4).Bytes(4, 'L');                                      // light map (a DDS in real files)
			Put(mapSize).Bytes(static_cast<size_t>(mapSize) * mapSize * 4, 0x40); // texture blend map
			Put<uint8_t>(15);                                                     // which textures are used
			Put<uint32_t>(3).Bytes(3, 'B');                                       // blend map DDS
			Put<uint32_t>(1);                                                     // one flair
			Put<uint32_t>(49).Put(0.5f).Put(x + 1).Put(7.0f).Put(z + 1).Put(0.0f).Put(1.5f).Put(0.0f);
			Put<uint8_t>(25).Put<uint8_t>(54).Put<uint8_t>(10).Put<uint8_t>(63);
			for (uint32_t i = 0; i < mapSize * mapSize; i++) Put<uint8_t>(i < mapSize * mapSize / 2 ? sceneA : sceneB);
			if (!mesh) { Put<uint32_t>(0); return; }
			Put<uint32_t>(2).Put<uint16_t>(65535).Put<uint16_t>(0);               // vertex usage
			for (int i = 0; i < 16; i++) Put<uint16_t>(4);                         // vertices per block
			for (int i = 0; i < 16; i++) { Put<uint16_t>(3); for (uint16_t v : { 0, 1, 2 }) Put(v); }
		}
	};

	std::string SampleRaw() {
		RawWriter w;
		w.Put<uint16_t>(32).Put<uint8_t>(0).Put<uint32_t>(2).Put<uint32_t>(2).Put<uint32_t>(1);
		w.Chunk(0, 3, -4.0f, 0.0f, 4, 1, 2, true);
		w.Chunk(1, 3, 0.0f, 0.0f, 4, 2, 3, false);
		return w.data;
	}

	bool Read(const std::string& data, Raw::Raw& raw) {
		std::istringstream stream(data);
		return Raw::ReadRaw(stream, raw);
	}
}

TEST(RawTerrainTests, ReadsEveryLayer) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(SampleRaw(), raw));
	EXPECT_EQ(raw.version, 32);
	ASSERT_EQ(raw.chunks.size(), 2u);
	const auto& chunk = raw.chunks[0];
	EXPECT_EQ(chunk.textureIds, (std::vector<uint32_t>{ 10, 11, 12, 13 }));
	EXPECT_FLOAT_EQ(chunk.scaleFactor, 2.0f);
	ASSERT_EQ(chunk.heightMap.size(), 9u);
	EXPECT_FLOAT_EQ(chunk.heightMap[8], 8.0f);
	EXPECT_EQ(chunk.colorMapResolution, 4u);
	EXPECT_EQ(chunk.colorMap.size(), 64u);
	EXPECT_EQ(chunk.lightMap.size(), 4u);
	EXPECT_EQ(chunk.textureMapResolution, 4u);
	EXPECT_EQ(chunk.textureMap[0], 0x40);
	EXPECT_EQ(chunk.textureSettings, 15);
	EXPECT_EQ(chunk.blendMap.size(), 3u);
	ASSERT_EQ(chunk.flairs.size(), 1u);
	EXPECT_EQ(chunk.flairs[0].id, 49u);
	EXPECT_FLOAT_EQ(chunk.flairs[0].position.x, -3.0f);
	EXPECT_FLOAT_EQ(chunk.flairs[0].rotation.y, 1.5f);
	EXPECT_EQ(chunk.flairs[0].colorG, 54);
	ASSERT_EQ(chunk.sceneMap.size(), 16u);
	EXPECT_EQ(chunk.vertSize, 2u);
	EXPECT_EQ(chunk.meshTri.size(), 16u);
	EXPECT_EQ(raw.chunks[1].vertSize, 0u);

	// The scene map runs like the heights: cell (i, j) is scene row i
	EXPECT_EQ(chunk.GetSceneIDAtGrid(0, 0), 1);
	EXPECT_EQ(chunk.GetSceneIDAtGrid(2, 2), 2);
	EXPECT_EQ(raw.chunks[1].GetSceneIDAtGrid(2, 0), 3);
	EXPECT_FLOAT_EQ(raw.minBoundsX, -4.0f);
	EXPECT_FLOAT_EQ(raw.maxBoundsX, 6.0f);
}

TEST(RawTerrainTests, RejectsDamagedFiles) {
	const auto raw = SampleRaw();
	for (const size_t length : { size_t{ 0 }, size_t{ 2 }, size_t{ 20 }, raw.size() / 2, raw.size() - 1 }) {
		Raw::Raw out;
		EXPECT_FALSE(Read(raw.substr(0, length), out)) << length;
	}
	// Sizes are capped before anything is allocated
	auto huge = raw;
	const uint32_t side = 100000;
	std::memcpy(huge.data() + 19, &side, sizeof(side)); // the first chunk's width
	Raw::Raw out;
	EXPECT_FALSE(Read(huge, out));
	EXPECT_FALSE(TerrainMap::Read(huge).has_value());
}

TEST(RawTerrainTests, TerrainMapUsesTheSameReader) {
	const auto grid = TerrainMap::Parse(SampleRaw());
	ASSERT_TRUE(grid.has_value());
	EXPECT_FLOAT_EQ(grid->minX, -4.0f);
	EXPECT_FLOAT_EQ(grid->step, 2.0f);
	EXPECT_EQ(grid->width, 5u);
	EXPECT_EQ(grid->height, 3u);
	// heights[3 * i + j] is at x = -4 + 2i, z = 2j; the second chunk overwrites the shared edge
	EXPECT_FLOAT_EQ(grid->heights[0], 0.0f);
	EXPECT_FLOAT_EQ(grid->heights[1 * 5 + 1], 4.0f);
	EXPECT_FLOAT_EQ(grid->maxY, 8.0f);
}
