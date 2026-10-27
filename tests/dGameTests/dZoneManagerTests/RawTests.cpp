#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

#include "Game.h"
#include "Logger.h"
#include "NiColor.h"
#include "Raw.h"
#include "SceneColor.h"
#include "TerrainTestWriter.h"

using namespace TerrainTestWriter;

class RawTest : public ::testing::Test {
protected:
	void SetUp() override {
		Game::logger = new Logger("./testing.log", false, false);
	}

	void TearDown() override {
		delete Game::logger;
		Game::logger = nullptr;
	}

	static bool Read(const std::string& data, Raw::Raw& raw) {
		std::istringstream stream(data);
		return Raw::ReadRaw(stream, raw);
	}

	// A chunksX by chunksZ grid of 5x5 point chunks, scale 2 (8 units across), each with a 4x4 scene map whose cells
	// are numbered [x * 4 + z] plus 16 * the chunk's index
	static std::vector<TestChunk> MakeGrid(uint32_t chunksX, uint32_t chunksZ, float originX = -8.0f, float originZ = -8.0f) {
		std::vector<TestChunk> chunks;
		for (uint32_t x = 0; x < chunksX; ++x) {
			for (uint32_t z = 0; z < chunksZ; ++z) {
				TestChunk chunk;
				chunk.id = x * chunksZ + z;
				chunk.width = 5;
				chunk.height = 5;
				chunk.scale = 2.0f;
				chunk.offsetX = originX + 8.0f * x;
				chunk.offsetZ = originZ + 8.0f * z;
				chunk.colorMapResolution = 4;
				for (uint8_t cell = 0; cell < 16; ++cell) chunk.sceneMap.push_back(static_cast<uint8_t>(cell + 16 * chunk.id));
				chunks.push_back(chunk);
			}
		}
		return chunks;
	}
};

TEST_F(RawTest, ReadsEveryPartOfVersion32) {
	auto chunks = MakeGrid(2, 3);
	chunks[1].vertSize = 0; // a chunk without a render mesh
	chunks[2].flairCount = 0;
	const auto data = WriteRaw(chunks, 32, 2, 3);

	std::istringstream stream(data);
	Raw::Raw raw;
	ASSERT_TRUE(Raw::ReadRaw(stream, raw));
	EXPECT_EQ(stream.tellg(), static_cast<std::streamoff>(data.size())); // every block was read or skipped

	EXPECT_EQ(raw.version, 32);
	EXPECT_EQ(raw.dev, 0);
	EXPECT_EQ(raw.numChunks, 6);
	EXPECT_EQ(raw.numChunksWidth, 3);
	EXPECT_EQ(raw.numChunksHeight, 2);
	ASSERT_EQ(raw.chunks.size(), 6);

	const auto& chunk = raw.chunks[4];
	EXPECT_EQ(chunk.id, 4);
	EXPECT_EQ(chunk.width, 5);
	EXPECT_EQ(chunk.height, 5);
	EXPECT_FLOAT_EQ(chunk.offsetX, 0.0f);
	EXPECT_FLOAT_EQ(chunk.offsetZ, 0.0f);
	EXPECT_FLOAT_EQ(chunk.scaleFactor, 2.0f);
	ASSERT_EQ(chunk.heightMap.size(), 25);
	EXPECT_FLOAT_EQ(chunk.heightMap[7], 7.0f);
	EXPECT_EQ(chunk.colorMapResolution, 4);
	ASSERT_EQ(chunk.sceneMap.size(), 16);
	EXPECT_EQ(chunk.sceneMap[5], 5 + 16 * 4);
	EXPECT_TRUE(chunk.IsValidForSceneLookup());
	EXPECT_FLOAT_EQ(chunk.sceneCellSizeX, 2.0f);
	EXPECT_FLOAT_EQ(chunk.sceneCellsPerUnitZ, 0.5f);

	EXPECT_EQ(chunk.textureIds, (std::vector<uint32_t>{ 100, 101, 102, 103 }));
	EXPECT_EQ(chunk.colorMap.size(), 4 * 4 * 4);
	EXPECT_EQ(chunk.colorMap[0], 0xAB);
	EXPECT_EQ(chunk.lightMap.size(), 5);
	EXPECT_EQ(chunk.textureMapResolution, 2);
	EXPECT_EQ(chunk.textureMap.size(), 2 * 2 * 4);
	EXPECT_EQ(chunk.textureSettings, 1);
	EXPECT_EQ(chunk.blendMap.size(), 7);

	ASSERT_EQ(chunk.flairs.size(), 2);
	const auto& flair = chunk.flairs[1];
	EXPECT_EQ(flair.id, 501);
	EXPECT_FLOAT_EQ(flair.scaleFactor, 1.5f);
	EXPECT_FLOAT_EQ(flair.position.x, 2.0f);
	EXPECT_FLOAT_EQ(flair.position.z, 3.0f);
	EXPECT_FLOAT_EQ(flair.rotation.y, 90.0f);
	EXPECT_EQ(flair.colorR, 10);
	EXPECT_EQ(flair.colorG, 20);
	EXPECT_EQ(flair.colorB, 30);
	EXPECT_EQ(flair.padding, 40);
	EXPECT_TRUE(raw.chunks[2].flairs.empty());

	EXPECT_EQ(chunk.vertSize, 4);
	EXPECT_EQ(chunk.meshVertUsage.size(), 4);
	EXPECT_EQ(chunk.meshVertSize.size(), 16);
	ASSERT_EQ(chunk.meshTri.size(), 16);
	EXPECT_EQ(chunk.meshTri[2].meshTriListSize, 6);
	EXPECT_EQ(chunk.meshTri[2].meshTriList.size(), 6);
	EXPECT_EQ(chunk.meshTri[15].meshTriListSize, 3);
	EXPECT_EQ(raw.chunks[1].vertSize, 0);
	EXPECT_TRUE(raw.chunks[1].meshTri.empty());
}

TEST_F(RawTest, BoundsAndChunkGridUseTheSpanBetweenFirstAndLastPoint) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(MakeGrid(2, 3), 32, 2, 3), raw));

	// 5 points 2 apart span 8 units, so chunks don't overlap
	EXPECT_FLOAT_EQ(raw.minBoundsX, -8.0f);
	EXPECT_FLOAT_EQ(raw.maxBoundsX, 8.0f);
	EXPECT_FLOAT_EQ(raw.minBoundsZ, -8.0f);
	EXPECT_FLOAT_EQ(raw.maxBoundsZ, 16.0f);
	EXPECT_FLOAT_EQ(raw.chunks[0].GetMaxX(), 0.0f);

	EXPECT_EQ(raw.chunkGridCountX, 2);
	EXPECT_EQ(raw.chunkGridCountZ, 3);
	EXPECT_FLOAT_EQ(raw.chunkSpanX, 8.0f);
	EXPECT_FLOAT_EQ(raw.chunkSpanZ, 8.0f);
	ASSERT_EQ(raw.chunkGrid.size(), 6);
	for (int32_t i = 0; i < 6; ++i) EXPECT_EQ(raw.chunkGrid[i], i);
}

TEST_F(RawTest, ChunksOutOfOrderAreFoundThroughTheGrid) {
	auto chunks = MakeGrid(2, 2);
	std::swap(chunks[0], chunks[3]);
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(chunks, 32, 2, 2), raw));
	ASSERT_EQ(raw.chunkGrid.size(), 4);
	EXPECT_EQ(raw.chunkGrid[0], 3);
	EXPECT_EQ(raw.chunkGrid[3], 0);
	EXPECT_EQ(raw.GetSceneIDAt(-7.0f, -7.0f), 1 * 4 + 1); // chunk id 0, local (1, 1)
}

TEST_F(RawTest, ChunksThatAreNotAGridAreSearchedOneByOne) {
	auto chunks = MakeGrid(2, 1);
	chunks[1].offsetZ = 3.0f; // shifted off the grid
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(chunks, 32, 2, 1), raw));
	EXPECT_TRUE(raw.chunkGrid.empty());
	EXPECT_EQ(raw.GetSceneIDAt(-7.0f, -7.0f), 1 * 4 + 1);
	EXPECT_EQ(raw.GetSceneIDAt(1.0f, 4.0f), 16 + 1 * 4 + 1); // chunk 1, local (1, 1)
}

TEST_F(RawTest, ReadsVersion31SceneMapWithoutTheLastRowAndColumn) {
	TestChunk chunk;
	chunk.width = 3;
	chunk.height = 3;
	// resolution is width - 1 = 2; the file has a 3x3 scene map
	chunk.sceneMap = { 1, 2, 9, 3, 4, 9, 9, 9, 9 };
	const auto data = WriteRaw({ chunk }, 31);

	std::istringstream stream(data);
	Raw::Raw raw;
	ASSERT_TRUE(Raw::ReadRaw(stream, raw));
	EXPECT_EQ(stream.tellg(), static_cast<std::streamoff>(data.size()));
	ASSERT_EQ(raw.chunks.size(), 1);
	EXPECT_EQ(raw.chunks[0].colorMapResolution, 2);
	EXPECT_EQ(raw.chunks[0].sceneMap, (std::vector<uint8_t>{ 1, 2, 3, 4 }));
	EXPECT_EQ(raw.chunks[0].sceneMapLegacy, chunk.sceneMap);
	EXPECT_EQ(raw.chunks[0].shaderId, 77);
	EXPECT_EQ(raw.chunks[0].textureIds, (std::vector<uint32_t>{ 100, 101, 102, 103 }));
	EXPECT_EQ(raw.chunks[0].colorMap.size(), 3 * 3 * 4); // width^2 pixels
	EXPECT_TRUE(raw.chunks[0].lightMap.empty());
	EXPECT_EQ(raw.chunks[0].textureMap.size(), 2 * 2 * 4);
	EXPECT_EQ(raw.chunks[0].flairs.size(), 2);
	EXPECT_TRUE(raw.chunks[0].meshTri.empty());
}

TEST_F(RawTest, Version30SceneMapIsReadButEveryCellIsSceneZero) {
	TestChunk chunk;
	chunk.width = 3;
	chunk.height = 3;
	// resolution is width - 1 = 2; the file has a 3x3 scene map the client doesn't use
	chunk.sceneMap = { 7, 7, 7, 7, 7, 7, 7, 7, 7 };
	const auto data = WriteRaw({ chunk, chunk }, 30, 1, 2);

	std::istringstream stream(data);
	Raw::Raw raw;
	ASSERT_TRUE(Raw::ReadRaw(stream, raw));
	EXPECT_EQ(stream.tellg(), static_cast<std::streamoff>(data.size()));
	ASSERT_EQ(raw.chunks.size(), 2);
	EXPECT_EQ(raw.chunks[1].sceneMapLegacy, chunk.sceneMap);
	EXPECT_EQ(raw.chunks[1].sceneMap, (std::vector<uint8_t>{ 0, 0, 0, 0 }));
	EXPECT_TRUE(raw.chunks[1].IsValidForSceneLookup());
	EXPECT_EQ(raw.GetSceneIDAt(0.5f, 0.5f), 0);
}

TEST_F(RawTest, DevFileHasNoChunks) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw({}, 32, 0, 0, 1), raw));
	EXPECT_EQ(raw.dev, 1);
	EXPECT_TRUE(raw.chunks.empty());
	EXPECT_EQ(raw.GetSceneIDAt(0.0f, 0.0f), std::nullopt);
}

TEST_F(RawTest, RejectsBadFiles) {
	Raw::Raw raw;
	EXPECT_FALSE(Read("", raw));

	const auto data = WriteRaw(MakeGrid(1, 2), 32, 1, 2);
	EXPECT_FALSE(Read(data.substr(0, data.size() - 3), raw)); // truncated in the last chunk's mesh

	std::string tooManyChunks = data;
	const uint32_t chunkCount = 5000;
	tooManyChunks.replace(3, sizeof(chunkCount), reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));
	EXPECT_FALSE(Read(tooManyChunks, raw));

	// a 3x3 chunk's color map resolution is at 15 (header) + 40 (chunk info) + 36 (heights)
	std::string hugeColorMap = WriteRaw({ TestChunk{} });
	const uint32_t resolution = 5000;
	hugeColorMap.replace(91, sizeof(resolution), reinterpret_cast<const char*>(&resolution), sizeof(resolution));
	EXPECT_FALSE(Read(hugeColorMap, raw));
}

TEST_F(RawTest, SceneCellIsTheNearestOneLikeTheClient) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(MakeGrid(1, 1, 0.0f, 0.0f)), raw));
	const auto& chunk = raw.chunks[0];

	// cells are 2 units; floor(position / 2 + 0.5), clamped to [0, 3]; index [x cell * 4 + z cell]
	EXPECT_EQ(chunk.GetSceneIDAtLocal(0.0f, 0.0f), 0);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(0.9f, 0.0f), 0);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(1.0f, 0.0f), 4);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(0.0f, 3.1f), 2);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(5.0f, 1.0f), 13);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(100.0f, 100.0f), 15);
	EXPECT_EQ(chunk.GetSceneIDAtLocal(-100.0f, -100.0f), 0);

	// grid points are 2 units apart: point (i, j) is at (2i, 2j)
	EXPECT_EQ(chunk.GetSceneIDAtGrid(1, 2), 1 * 4 + 2);
	EXPECT_EQ(chunk.GetSceneIDAtGrid(4, 4), 15);
}

TEST_F(RawTest, SceneAtPositionMatchesTheClient) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(MakeGrid(2, 2), 32, 2, 2), raw));

	// inside chunk 0 ([-8, 0] x [-8, 0])
	EXPECT_EQ(raw.GetSceneIDAt(-8.0f, -8.0f), 0);
	EXPECT_EQ(raw.GetSceneIDAt(-5.0f, -8.0f), 4 * 2);
	// chunk 3 ([0, 8] x [0, 8])
	EXPECT_EQ(raw.GetSceneIDAt(0.5f, 0.5f), 48);
	// within 0.001 of a chunk's start counts as the next chunk
	EXPECT_EQ(raw.GetSceneIDAt(-0.0005f, -7.0f), 32 + 1);
	EXPECT_EQ(raw.GetSceneIDAt(-0.01f, -7.0f), 3 * 4 + 1);
	// outside the terrain clamps to its edge
	EXPECT_EQ(raw.GetSceneIDAt(-500.0f, -500.0f), 0);
	// the far edge is kept one scene cell inside, so it is the last cell of the last chunk
	EXPECT_EQ(raw.GetSceneIDAt(8.0f, 8.0f), 48 + 15);
	EXPECT_EQ(raw.GetSceneIDAt(500.0f, 500.0f), 48 + 15);
}

TEST_F(RawTest, GridLookupMatchesCheckingEveryChunk) {
	auto chunks = MakeGrid(3, 4, -1126.4f, -921.6f);
	for (auto& chunk : chunks) {
		chunk.scale = 3.2f;
		chunk.offsetX = -1126.4f + (chunk.offsetX + 1126.4f) / 8.0f * 12.8f;
		chunk.offsetZ = -921.6f + (chunk.offsetZ + 921.6f) / 8.0f * 12.8f;
	}
	chunks.erase(chunks.begin() + 5); // a hole in the grid
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(chunks, 32, 3, 4), raw));
	ASSERT_FALSE(raw.chunkGrid.empty());

	Raw::Raw linear = raw;
	linear.chunkGrid.clear();

	std::mt19937 random(1);
	std::uniform_real_distribution<float> randomX(raw.minBoundsX - 20.0f, raw.maxBoundsX + 20.0f);
	std::uniform_real_distribution<float> randomZ(raw.minBoundsZ - 20.0f, raw.maxBoundsZ + 20.0f);
	for (int i = 0; i < 20000; ++i) {
		float x = randomX(random);
		float z = randomZ(random);
		// every few positions on a chunk edge
		if (i % 4 == 0) x = raw.minBoundsX + std::round((x - raw.minBoundsX) / raw.chunkSpanX) * raw.chunkSpanX;
		if (i % 5 == 0) z = raw.minBoundsZ + std::round((z - raw.minBoundsZ) / raw.chunkSpanZ) * raw.chunkSpanZ;
		ASSERT_EQ(raw.GetSceneIDAt(x, z), linear.GetSceneIDAt(x, z)) << x << ", " << z;
	}
}

TEST_F(RawTest, GridToWorldPosition) {
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw(MakeGrid(1, 1)), raw));
	const auto position = raw.chunks[0].GridToWorldPos(1, 3);
	EXPECT_FLOAT_EQ(position.x, -8.0f + 2.0f);
	EXPECT_FLOAT_EQ(position.y, 1 * 5 + 3);
	EXPECT_FLOAT_EQ(position.z, -8.0f + 6.0f);
}

TEST_F(RawTest, TerrainMeshForNonSquareChunks) {
	TestChunk first;
	first.width = 3;
	first.height = 4;
	first.colorMapResolution = 2;
	first.sceneMap = { 1, 2, 3, 4 };
	TestChunk second = first;
	second.offsetX = 2.0f;
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw({ first, second }, 32, 2, 1), raw));

	Raw::TerrainMesh mesh;
	Raw::GenerateTerrainMesh(raw, mesh);
	ASSERT_EQ(mesh.vertices.size(), 2 * 12);
	ASSERT_EQ(mesh.triangles.size(), 2 * (2 * 3) * 2 * 3);
	for (const auto index : mesh.triangles) EXPECT_LT(index, mesh.vertices.size());

	// vertex [i * height + j]; the first quad of the first chunk
	EXPECT_EQ(mesh.triangles[0], 1 * 4 + 1);
	EXPECT_EQ(mesh.triangles[1], 1 * 4 + 0);
	EXPECT_EQ(mesh.triangles[2], 0);
	EXPECT_FLOAT_EQ(mesh.vertices[1 * 4 + 3].position.x, 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[1 * 4 + 3].position.z, 3.0f);
	EXPECT_EQ(mesh.vertices[2 * 4 + 3].sceneID, 4);
	// the second chunk's vertices come after the first's
	EXPECT_FLOAT_EQ(mesh.vertices[12].position.x, 2.0f);
	EXPECT_EQ(mesh.triangles[36], 12 + 1 * 4 + 1);
}

TEST_F(RawTest, NoTerrainMeshWithoutSceneData) {
	TestChunk chunk;
	Raw::Raw raw;
	ASSERT_TRUE(Read(WriteRaw({ chunk }, 30), raw));
	Raw::TerrainMesh mesh;
	Raw::GenerateTerrainMesh(raw, mesh);
	EXPECT_TRUE(mesh.vertices.empty());
	EXPECT_TRUE(mesh.triangles.empty());
}

TEST_F(RawTest, WritesTheTerrainMeshAsObj) {
	Raw::TerrainMesh mesh;
	mesh.vertices = { Raw::SceneVertex(NiPoint3(0.0f, 1.0f, 2.0f), 1), Raw::SceneVertex(NiPoint3(3.0f, 4.0f, 5.0f), 200), Raw::SceneVertex() };
	mesh.triangles = { 0, 1, 2 };
	const auto path = (std::filesystem::temp_directory_path() / "dlu_raw_test.obj").string();
	ASSERT_TRUE(Raw::WriteTerrainMeshToOBJ(mesh, path));

	std::vector<std::string> lines;
	{
		std::ifstream file(path);
		for (std::string line; std::getline(file, line);) lines.push_back(line);
	} // closed before removing it (Windows can't remove an open file)
	std::filesystem::remove(path);

	ASSERT_EQ(lines.size(), 4);
	EXPECT_EQ(lines[0], "v 0 1 2 1 0 0"); // scene 1 is red
	EXPECT_EQ(lines[1], "v 3 4 5 1 1 1"); // past the table: white
	EXPECT_EQ(lines[3], "f 1 2 3");
	EXPECT_FALSE(Raw::WriteTerrainMeshToOBJ(mesh, "/nonexistent-directory/terrain.obj"));
}

TEST(SceneColorTest, TemplateColorsAndFallback) {
	EXPECT_FLOAT_EQ(SceneColor::Get(0).m_Red, 128.0f / 255.0f);
	EXPECT_FLOAT_EQ(SceneColor::Get(3).m_Blue, 1.0f);
	EXPECT_FLOAT_EQ(SceneColor::Get(145).m_Green, SceneColor::TEMPLATE_COLORS.back().m_Green);
	EXPECT_EQ(&SceneColor::Get(146), &SceneColor::FALLBACK_COLOR);
	EXPECT_EQ(&SceneColor::Get(255), &SceneColor::FALLBACK_COLOR);
}

TEST(NiColorTest, ConstructionAndLuminance) {
	constexpr NiColor black;
	EXPECT_FLOAT_EQ(black.m_Red, 0.0f);
	EXPECT_FLOAT_EQ(black.Luminance(), 0.0f);
	EXPECT_NEAR(NiColor(1.0f, 1.0f, 1.0f).Luminance(), 1.0f, 0.0001f);
	EXPECT_FLOAT_EQ(NiColor(0.0f, 1.0f, 0.0f).Luminance(), 0.71516f);
}
