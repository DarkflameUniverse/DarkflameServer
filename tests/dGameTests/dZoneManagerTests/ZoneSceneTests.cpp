#include "GameDependencies.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "AssetManager.h"
#include "CDBrickIDTableTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDZoneTableTable.h"
#include "DEVGMCommands.h"
#include "Entity.h"
#include "TerrainTestWriter.h"
#include "Zone.h"
#include "dZoneManager.h"

using namespace TerrainTestWriter;

// The zone table's storage (CDZoneTableTable.cpp), to add the test zone
namespace CDZoneTableTable {
	extern Table entries;
}

// Loads a written test zone (a .luz and its .raw) through the zone manager
class ZoneSceneTest : public GameDependenciesTest {
protected:
	static constexpr LWOMAPID TEST_MAP = 9990;
	std::filesystem::path root;

	void SetUp() override {
		SetUpDependencies();
		root = std::filesystem::temp_directory_path() / (std::string("dlu_zone_scene_test_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
		std::filesystem::create_directories(root / "res" / "maps" / "testzone");
		std::ofstream(root / "res" / "CDServer.sqlite").close(); // marks an unpacked client
		Game::assetManager = new AssetManager(root);

		CDZoneTable zone{};
		zone.zoneID = TEST_MAP;
		zone.zoneName = "testzone/test.luz";
		CDZoneTableTable::entries[TEST_MAP] = zone;
	}

	void TearDown() override {
		CDZoneTableTable::entries.erase(TEST_MAP);
		delete Game::assetManager;
		Game::assetManager = nullptr;
		std::filesystem::remove_all(root);
		TearDownDependencies();
	}

	void WriteFile(const std::string& name, const std::string& data) const {
		std::ofstream file(root / "res" / "maps" / "testzone" / name, std::ios::binary);
		file.write(data.data(), data.size());
	}

	// Scenes 0 (global), 1, 2 and 3 (general) and 5 (audio, layer 1); transitions 1 <-> 2 and 2 <-> 3
	void WriteZone(const std::string& rawName = "test.raw", uint8_t boundaryCount = 2) const {
		const std::vector<TestScene> scenes = {
			{ "global.lvl", 0, 0, "Global", 10, 20, 30 },
			{ "one.lvl", 1, 0, "Scene One", 255, 0, 0 },
			{ "two.lvl", 2, 0, "Scene Two", 0, 255, 0 },
			{ "three.lvl", 3, 0, "Scene Three", 0, 0, 255 },
			{ "audio.lvl", 5, 1, "Audio", 0, 0, 0 },
		};
		WriteFile("test.luz", WriteLuz(TEST_MAP, scenes, boundaryCount, rawName, {
			{ { 1, 0, NiPoint3(1.0f, 0.0f, 0.0f) }, { 2, 0, NiPoint3(2.0f, 0.0f, 0.0f) } },
			{ { 2, 0, NiPoint3(3.0f, 0.0f, 0.0f) }, { 3, 0, NiPoint3(4.0f, 0.0f, 0.0f) } },
		}));
	}

	// One 3x3 point chunk from (0, 0) to (4, 4) whose 2x2 scene map holds scenes 1 2 / 3 1
	static std::string MakeRaw() {
		TestChunk chunk;
		chunk.width = 3;
		chunk.height = 3;
		chunk.scale = 2.0f;
		chunk.colorMapResolution = 2;
		chunk.sceneMap = { 1, 2, 3, 1 };
		return WriteRaw({ chunk });
	}

	Zone* LoadZone() const {
		Game::zoneManager->LoadZone(LWOZONEID(TEST_MAP, 0, 0));
		Zone* zone = Game::zoneManager->GetZoneMut();
		zone->LoadZoneIntoMemory(); // the levels the scenes name aren't written
		Game::zoneManager->BuildSceneGraph();
		return zone;
	}

	static bool Contains(const std::vector<LWOSCENEID>& scenes, LWOSCENEID scene) {
		return std::ranges::find(scenes, scene) != scenes.end();
	}
};

TEST_F(ZoneSceneTest, ReadsScenesBoundariesAndTransitions) {
	WriteZone();
	WriteFile("test.raw", MakeRaw());
	const Zone* zone = LoadZone();

	EXPECT_EQ(zone->GetWorldID(), TEST_MAP);
	EXPECT_EQ(zone->GetZoneRawPath(), "test.raw");
	EXPECT_EQ(zone->GetZoneName(), "Test Zone"); // read after the skipped boundary lines
	EXPECT_EQ(zone->GetZonePath(), "maps/testzone/");

	ASSERT_EQ(zone->GetScenes().size(), 5);
	const SceneRef* one = zone->GetScene(LWOSCENEID(1, 0));
	ASSERT_NE(one, nullptr);
	EXPECT_EQ(one->name, "Scene One");
	EXPECT_EQ(one->sceneType, eSceneType::General);
	EXPECT_FLOAT_EQ(one->color.m_Red, 1.0f);
	const SceneRef* global = zone->GetScene(LWOSCENEID(0, 0));
	ASSERT_NE(global, nullptr);
	// stored in the file as red, blue, green
	EXPECT_FLOAT_EQ(global->color.m_Red, 10 / 255.0f);
	EXPECT_FLOAT_EQ(global->color.m_Green, 20 / 255.0f);
	EXPECT_FLOAT_EQ(global->color.m_Blue, 30 / 255.0f);
	const SceneRef* audio = zone->GetScene(LWOSCENEID(5, 1));
	ASSERT_NE(audio, nullptr);
	EXPECT_EQ(audio->sceneType, eSceneType::Audio);
	EXPECT_EQ(zone->GetScene(LWOSCENEID(4, 0)), nullptr);

	const auto& transitions = zone->GetSceneTransitions();
	ASSERT_EQ(transitions.size(), 2);
	ASSERT_EQ(transitions[1].points.size(), 2);
	EXPECT_EQ(transitions[1].points[0].sceneID, LWOSCENEID(2, 0));
	EXPECT_EQ(transitions[1].points[1].sceneID, LWOSCENEID(3, 0));
	EXPECT_FLOAT_EQ(transitions[1].points[1].position.x, 4.0f);
}

TEST_F(ZoneSceneTest, ZoneWithoutBoundaryLines) {
	WriteZone("test.raw", 0);
	WriteFile("test.raw", MakeRaw());
	const Zone* zone = LoadZone();
	EXPECT_EQ(zone->GetZoneName(), "Test Zone");
	EXPECT_EQ(zone->GetSceneTransitions().size(), 2);
}

TEST_F(ZoneSceneTest, SceneAtPosition) {
	WriteZone();
	WriteFile("test.raw", MakeRaw());
	LoadZone();

	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(0.5f, 100.0f, 0.5f)), LWOSCENEID(1, 0));
	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(0.5f, -50.0f, 3.5f)), LWOSCENEID(2, 0));
	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(3.5f, 0.0f, 0.5f)), LWOSCENEID(3, 0));
	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(50.0f, 0.0f, 50.0f)), LWOSCENEID(1, 0));
}

TEST_F(ZoneSceneTest, SceneGraphFromTransitions) {
	WriteZone();
	WriteFile("test.raw", MakeRaw());
	LoadZone();

	const auto& graph = Game::zoneManager->GetSceneAdjacencyList();
	EXPECT_EQ(graph.size(), 4); // the audio scene is left out
	EXPECT_FALSE(graph.contains(LWOSCENEID(5, 1)));

	const auto one = Game::zoneManager->GetAdjacentScenes(LWOSCENEID(1, 0));
	EXPECT_EQ(one.size(), 2);
	EXPECT_TRUE(Contains(one, LWOSCENEID(2, 0)));
	EXPECT_TRUE(Contains(one, LWOSCENEID(0, 0)));

	const auto two = Game::zoneManager->GetAdjacentScenes(LWOSCENEID(2, 0));
	EXPECT_EQ(two.size(), 3);
	EXPECT_TRUE(Contains(two, LWOSCENEID(1, 0)));
	EXPECT_TRUE(Contains(two, LWOSCENEID(3, 0)));

	// the global scene is next to every other scene
	const auto global = Game::zoneManager->GetAdjacentScenes(LWOSCENEID(0, 0));
	EXPECT_EQ(global.size(), 3);
	EXPECT_FALSE(Contains(global, LWOSCENEID(0, 0)));

	EXPECT_TRUE(Game::zoneManager->GetAdjacentScenes(LWOSCENEID(9, 0)).empty());

	// building again doesn't add anything twice
	Game::zoneManager->BuildSceneGraph();
	EXPECT_EQ(Game::zoneManager->GetAdjacentScenes(LWOSCENEID(2, 0)).size(), 3);
}

TEST_F(ZoneSceneTest, MissingRawStillLoadsTheZone) {
	WriteZone("missing.raw");
	const Zone* zone = LoadZone();
	EXPECT_EQ(zone->GetZoneName(), "Test Zone");
	EXPECT_EQ(zone->GetSceneTransitions().size(), 2);
	EXPECT_TRUE(zone->GetZoneRaw().chunks.empty());
	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(0.5f, 0.0f, 0.5f)), LWOSCENEID_INVALID);
}

TEST_F(ZoneSceneTest, UnreadableRawStillLoadsTheZone) {
	WriteZone();
	const auto raw = MakeRaw();
	WriteFile("test.raw", raw.substr(0, raw.size() / 2));
	const Zone* zone = LoadZone();
	EXPECT_EQ(zone->GetZoneName(), "Test Zone");
	EXPECT_TRUE(zone->GetZoneRaw().chunks.empty());
	EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(NiPoint3(0.5f, 0.0f, 0.5f)), LWOSCENEID_INVALID);
}

TEST_F(ZoneSceneTest, ScenePointCommandsSpawnBricks) {
	WriteZone();
	WriteFile("test.raw", MakeRaw());
	LoadZone();

	auto& bricks = CDClientManager::GetEntriesMutable<CDBrickIDTableTable>();
	bricks = { { 40, 3005 }, { 33, 3001 }, { 35, 3003 } };
	// the bricks have no components, so creating them needs no client database
	for (const LOT lot : { 33, 35, 40 }) CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>()[lot] = 0;

	EntityInfo playerInfo = info;
	playerInfo.pos = NiPoint3(0.5f, 0.0f, 0.5f); // scene 1
	auto player = std::make_unique<Entity>(1, playerInfo);

	// Grid point (i, j) is at (2i, 2j); its scene cell is (min(i, 1), min(j, 1)), so of the 9 points scene 1 has 5,
	// scene 2 has 2 ((0, 1), (0, 2)) and scene 3 has 2 ((1, 0), (2, 0))
	DEVGMCommands::SpawnScenePoints(player.get(), UNASSIGNED_SYSTEM_ADDRESS, "1");
	// bricks sorted by LOT: 33, 35, 40; scene 1 -> 35, scene 2 -> 40, scene 3 -> 33
	const auto sceneOne = Game::entityManager->GetEntitiesByLOT(35);
	EXPECT_EQ(sceneOne.size(), 5);
	for (const auto* brick : sceneOne) EXPECT_EQ(Game::zoneManager->GetSceneIDFromPosition(brick->GetPosition()), LWOSCENEID(1, 0));
	EXPECT_TRUE(Game::entityManager->GetEntitiesByLOT(40).empty());

	// the default step of 8 only samples point (0, 0)
	DEVGMCommands::SpawnAllScenePoints(player.get(), UNASSIGNED_SYSTEM_ADDRESS, "");
	EXPECT_EQ(Game::entityManager->GetEntitiesByLOT(35).size(), 5 + 1);
	EXPECT_TRUE(Game::entityManager->GetEntitiesByLOT(40).empty());

	DEVGMCommands::SpawnAllScenePoints(player.get(), UNASSIGNED_SYSTEM_ADDRESS, "1");
	EXPECT_EQ(Game::entityManager->GetEntitiesByLOT(35).size(), 6 + 5);
	EXPECT_EQ(Game::entityManager->GetEntitiesByLOT(40).size(), 2);
	EXPECT_EQ(Game::entityManager->GetEntitiesByLOT(33).size(), 2);

	// the scene commands only report; they must not fail with or without scene data
	DEVGMCommands::GetScene(player.get(), UNASSIGNED_SYSTEM_ADDRESS, "");
	DEVGMCommands::GetAdjacentScenes(player.get(), UNASSIGNED_SYSTEM_ADDRESS, "");
	bricks.clear();
}
