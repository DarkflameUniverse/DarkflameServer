#include <gtest/gtest.h>

#include "ZoneScenes.h"

namespace {
	// A chunk `cells` scene map cells wide covering (x, z) .. (x + 64, z + 64): 65 height samples, 1 unit apart
	Raw::Chunk MakeChunk(float x, float z, uint32_t cells, std::vector<uint8_t> scenes) {
		Raw::Chunk chunk{};
		chunk.width = chunk.height = 65;
		chunk.offsetX = x;
		chunk.offsetZ = z;
		chunk.scaleFactor = 1.0f;
		chunk.heightMap.assign(65 * 65, 0.0f);
		chunk.colorMapResolution = cells;
		chunk.sceneMap = std::move(scenes);
		return chunk;
	}

	SceneTransition Transition(uint32_t a, uint32_t b, uint32_t layer = 0) {
		SceneTransition transition;
		transition.points.push_back({ (static_cast<uint64_t>(layer) << 32) | a, {} });
		transition.points.push_back({ b, {} });
		return transition;
	}

	std::vector<ZoneScene> Scenes(std::initializer_list<uint32_t> ids) {
		std::vector<ZoneScene> scenes;
		for (const auto id : ids) {
			ZoneScene scene;
			scene.id = id;
			scenes.push_back(scene);
		}
		return scenes;
	}
}

// Cells are x major (index x * resolution + z) and a position takes the nearest cell, as the client's lookup does
TEST(ZoneScenesTests, FindsTheSceneUnderAPosition) {
	Raw::Raw raw{};
	// 2x2 cells, each 32 units: x = 0 has scenes 1 (z low) and 2, x = 1 has 3 and no scene
	raw.chunks.push_back(MakeChunk(0, 0, 2, { 1, 2, 3, ZoneScenes::NO_SCENE }));
	raw.chunks.push_back(MakeChunk(64, 0, 1, { 7 }));
	const ZoneScenes::SceneMap map(raw);
	ASSERT_FALSE(map.Empty());
	EXPECT_EQ(map.SceneAt(1, 1), 1u);
	EXPECT_EQ(map.SceneAt(1, 17), 2u);   // past half a cell: the next cell
	EXPECT_EQ(map.SceneAt(17, 1), 3u);
	EXPECT_EQ(map.SceneAt(17, 17), ZoneScenes::GLOBAL_SCENE); // no scene there
	EXPECT_EQ(map.SceneAt(100, 10), 7u);
	// Off the terrain: clamped to its edge
	EXPECT_EQ(map.SceneAt(-50, -50), 1u);
	EXPECT_EQ(map.SceneAt(500, 10), 7u);
	EXPECT_EQ(ZoneScenes::SceneMap().SceneAt(1, 1), ZoneScenes::GLOBAL_SCENE);
}

TEST(ZoneScenesTests, LoadsTheConnectedScenes) {
	// 1-2 and 2-3 connect; 9 isn't in the zone, so its transition is dropped
	const ZoneScenes::SceneGraph graph(Scenes({ 0, 1, 2, 3, 4 }), { Transition(1, 2), Transition(3, 2, 1), Transition(2, 9), Transition(4, 4) });
	EXPECT_EQ(graph.Loaded(1), (std::set<uint32_t>{ 0, 1, 2 }));
	EXPECT_EQ(graph.Loaded(2), (std::set<uint32_t>{ 0, 1, 2, 3 }));
	EXPECT_EQ(graph.Loaded(4), (std::set<uint32_t>{ 0, 4 }));
	EXPECT_EQ(graph.Loaded(ZoneScenes::GLOBAL_SCENE), (std::set<uint32_t>{ 0 }));
	EXPECT_TRUE(graph.Neighbours(9).empty());
}

TEST(ZoneScenesTests, SendsSceneMapsAsRuns) {
	std::vector<uint8_t> cells(300, 4);
	cells[0] = 1;
	cells[299] = 2;
	// 1, then 298 fours (a run is at most 255 long), then 2; only the first `count` cells count
	EXPECT_EQ(ZoneScenes::RunLengths(cells, 300), std::string("\x01\x01\xFF\x04\x2B\x04\x01\x02", 8));
	EXPECT_EQ(ZoneScenes::RunLengths(cells, 1), std::string("\x01\x01", 2));
	EXPECT_EQ(ZoneScenes::RunLengths(cells, 1000).size(), 8u);
}

TEST(ZoneScenesTests, GhostsObjectsByTheirScene) {
	const std::set<uint32_t> loaded{ 0, 1, 2 };
	EXPECT_TRUE(ZoneScenes::InLoadedScene(2, 7, loaded));   // placed in a loaded scene, wherever it is now
	EXPECT_FALSE(ZoneScenes::InLoadedScene(3, 1, loaded));  // placed in one that isn't
	EXPECT_TRUE(ZoneScenes::InLoadedScene(0, 9, loaded));   // the global scene's objects always
	EXPECT_TRUE(ZoneScenes::InLoadedScene(-1, 1, loaded));  // spawned at run time: the scene under it
	EXPECT_FALSE(ZoneScenes::InLoadedScene(-1, 5, loaded));
}
