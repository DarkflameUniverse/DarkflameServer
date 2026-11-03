#include "GameDependencies.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "AssetManager.h"
#include "CDZoneTableTable.h"
#include "Zone.h"
#include "dZoneManager.h"
#include "eWaypointCommandType.h"

// The zone table's storage (CDZoneTableTable.cpp), to add the test zone
namespace CDZoneTableTable {
	extern Table entries;
}

namespace {
	template<typename T>
	void Write(std::ostream& stream, const T& value) {
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void WriteString8(std::ostream& stream, const std::string& value) {
		Write<uint8_t>(stream, static_cast<uint8_t>(value.size()));
		stream.write(value.data(), value.size());
	}

	// A u8 length, then that many UTF-16 characters (ASCII only)
	void WriteWString8(std::ostream& stream, const std::string& value) {
		Write<uint8_t>(stream, static_cast<uint8_t>(value.size()));
		for (const char c : value) Write<uint16_t>(stream, static_cast<uint16_t>(c));
	}

	// The waypoint every version 2 path has: rotation, the moving platform values and one command
	void WriteOldWaypoint(std::ostream& stream, float x) {
		Write(stream, NiPoint3(x, 0.0f, 0.0f));
		for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(stream, value); // rotation
		Write<uint8_t>(stream, 1); // lock player
		Write(stream, 3.0f); // speed
		Write(stream, 4.0f); // wait
		Write<uint32_t>(stream, 1);
		WriteWString8(stream, "delay");
		WriteWString8(stream, "2");
	}

	void WritePathHeader(std::ostream& stream, uint32_t version, const std::string& name, PathType type) {
		Write(stream, version);
		WriteWString8(stream, name);
		Write(stream, type);
		Write<uint32_t>(stream, 0); // flags
		Write<uint32_t>(stream, 0); // behavior
	}
}

// Loads a written .luz (version 41, no scenes) with the given path chunk through the zone manager
class ZonePathTest : public GameDependenciesTest {
protected:
	static constexpr LWOMAPID TEST_MAP = 9992;
	std::filesystem::path root;

	void SetUp() override {
		SetUpDependencies();
		root = std::filesystem::temp_directory_path() / (std::string("dlu_zone_path_test_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
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

	const Zone* LoadZoneWithPaths(uint32_t pathCount, const std::string& paths) const {
		std::ostringstream stream;
		Write<uint32_t>(stream, 41); // version
		Write<uint32_t>(stream, 1); // revision
		Write<uint32_t>(stream, TEST_MAP);
		Write(stream, NiPoint3(0.0f, 0.0f, 0.0f)); // spawn position
		for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(stream, value); // spawn rotation
		Write<uint32_t>(stream, 0); // scenes
		Write<uint8_t>(stream, 0); // boundary lines
		WriteString8(stream, "test.raw");
		WriteString8(stream, "Test Zone");
		WriteString8(stream, "A zone for tests");
		Write<uint32_t>(stream, 0); // transitions
		Write<uint32_t>(stream, static_cast<uint32_t>(8 + paths.size())); // path data length
		Write<uint32_t>(stream, 1); // path chunk version
		Write(stream, pathCount);
		stream.write(paths.data(), paths.size());
		const auto data = stream.str();
		std::ofstream(root / "res" / "maps" / "testzone" / "test.luz", std::ios::binary).write(data.data(), data.size());

		Game::zoneManager->LoadZone(LWOZONEID(TEST_MAP, 0, 0));
		Zone* zone = Game::zoneManager->GetZoneMut();
		zone->LoadZoneIntoMemory();
		return zone;
	}
};

// Before version 3 a path names its type and every waypoint has the moving platform values and a command list
TEST_F(ZonePathTest, ReadsVersion2Paths) {
	std::ostringstream paths;
	Write<uint32_t>(paths, 2);
	WriteWString8(paths, "Lava");
	WriteWString8(paths, "npc");
	Write<uint32_t>(paths, 0); // flags
	Write<uint32_t>(paths, 0); // behavior
	Write<uint32_t>(paths, 2);
	WriteOldWaypoint(paths, 1.0f);
	WriteOldWaypoint(paths, 2.0f);
	Write<uint32_t>(paths, 2);
	WriteWString8(paths, "plat");
	WriteWString8(paths, "platform");
	Write<uint32_t>(paths, 0);
	Write<uint32_t>(paths, 0);
	Write<uint32_t>(paths, 1);
	WriteOldWaypoint(paths, 5.0f);
	const Zone* zone = LoadZoneWithPaths(2, paths.str());

	const Path* lava = zone->GetPath("Lava");
	ASSERT_NE(lava, nullptr);
	EXPECT_EQ(lava->pathType, PathType::Movement);
	ASSERT_EQ(lava->pathWaypoints.size(), 2);
	EXPECT_FLOAT_EQ(lava->pathWaypoints[1].position.x, 2.0f);
	EXPECT_FLOAT_EQ(lava->pathWaypoints[1].speed, 3.0f);
	ASSERT_EQ(lava->pathWaypoints[1].commands.size(), 1);
	EXPECT_EQ(lava->pathWaypoints[1].commands[0].command, eWaypointCommandType::DELAY);

	const Path* platform = zone->GetPath("plat");
	ASSERT_NE(platform, nullptr);
	EXPECT_EQ(platform->pathType, PathType::MovingPlatform);
	ASSERT_EQ(platform->pathWaypoints.size(), 1);
	EXPECT_FLOAT_EQ(platform->pathWaypoints[0].movingPlatform.wait, 4.0f);
}

// Spawner and camera paths have no header before version 4; the spawner's network flag starts at version 9
TEST_F(ZonePathTest, SpawnerAndCameraHeadersStartAtVersion4) {
	std::ostringstream paths;
	WritePathHeader(paths, 3, "spawn3", PathType::Spawner);
	Write<uint32_t>(paths, 1);
	Write(paths, NiPoint3(7.0f, 0.0f, 0.0f));
	for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(paths, value);
	Write<uint32_t>(paths, 0); // config
	WritePathHeader(paths, 8, "spawn8", PathType::Spawner);
	Write<uint32_t>(paths, 1234); // LOT
	Write<uint32_t>(paths, 10); // respawn time
	Write<int32_t>(paths, 5); // max to spawn
	Write<uint32_t>(paths, 2); // amount maintained
	Write<uint64_t>(paths, 99); // spawner object ID
	Write<uint32_t>(paths, 0); // no waypoints
	WritePathHeader(paths, 3, "cam3", PathType::Camera);
	Write<uint32_t>(paths, 0);
	const Zone* zone = LoadZoneWithPaths(3, paths.str());

	const Path* spawn3 = zone->GetPath("spawn3");
	ASSERT_NE(spawn3, nullptr);
	EXPECT_EQ(spawn3->spawner.spawnedLOT, 0);
	ASSERT_EQ(spawn3->pathWaypoints.size(), 1);
	EXPECT_FLOAT_EQ(spawn3->pathWaypoints[0].position.x, 7.0f);

	const Path* spawn8 = zone->GetPath("spawn8");
	ASSERT_NE(spawn8, nullptr);
	EXPECT_EQ(spawn8->spawner.spawnedLOT, 1234);
	EXPECT_EQ(spawn8->spawner.spawnerObjID, 99);

	const Path* cam3 = zone->GetPath("cam3");
	ASSERT_NE(cam3, nullptr);
	EXPECT_TRUE(cam3->camera.nextPath.empty());
}

// Before version 11 a camera waypoint's field of view is 1.25 times larger
TEST_F(ZonePathTest, OldCameraFieldOfViewIsScaled) {
	std::ostringstream paths;
	for (const auto& [version, name] : { std::pair<uint32_t, std::string>{ 10, "cam10" }, { 11, "cam11" } }) {
		WritePathHeader(paths, version, name, PathType::Camera);
		WriteWString8(paths, "next");
		Write<uint32_t>(paths, 1);
		Write(paths, NiPoint3(9.0f, 0.0f, 0.0f));
		for (const float value : { 1.0f, 0.0f, 0.0f, 0.0f }) Write(paths, value);
		for (const float value : { 1.0f, 50.0f, 0.1f, 0.2f, 0.3f }) Write(paths, value); // time, fov, tension, continuity, bias
	}
	const Zone* zone = LoadZoneWithPaths(2, paths.str());

	const Path* cam10 = zone->GetPath("cam10");
	ASSERT_NE(cam10, nullptr);
	EXPECT_EQ(cam10->camera.nextPath, "next");
	ASSERT_EQ(cam10->pathWaypoints.size(), 1);
	EXPECT_FLOAT_EQ(cam10->pathWaypoints[0].camera.fov, 40.0f);
	EXPECT_FLOAT_EQ(cam10->pathWaypoints[0].camera.bias, 0.3f);
	const Path* cam11 = zone->GetPath("cam11");
	ASSERT_NE(cam11, nullptr);
	EXPECT_FLOAT_EQ(cam11->pathWaypoints[0].camera.fov, 50.0f);
}

// Like the client, a path it can't read leaves the zone with no paths
TEST_F(ZonePathTest, UnreadablePathLeavesNoPaths) {
	for (const uint32_t badVersion : { 19u, 25u }) {
		std::ostringstream paths;
		WritePathHeader(paths, 18, "ok", PathType::Movement);
		Write<uint32_t>(paths, 0); // no waypoints
		Write(paths, badVersion);
		const Zone* zone = LoadZoneWithPaths(2, paths.str());
		EXPECT_EQ(zone->GetPath("ok"), nullptr) << badVersion;
		EXPECT_EQ(zone->GetZoneName(), "Test Zone") << badVersion;
	}

	std::ostringstream tooManyCommands;
	WritePathHeader(tooManyCommands, 18, "commands", PathType::Movement);
	Write<uint32_t>(tooManyCommands, 1);
	Write(tooManyCommands, NiPoint3(0.0f, 0.0f, 0.0f));
	Write<uint32_t>(tooManyCommands, 100);
	EXPECT_EQ(LoadZoneWithPaths(1, tooManyCommands.str())->GetPath("commands"), nullptr);

	std::ostringstream tooManyWaypoints;
	WritePathHeader(tooManyWaypoints, 18, "waypoints", PathType::Movement);
	Write<uint32_t>(tooManyWaypoints, 10000);
	EXPECT_EQ(LoadZoneWithPaths(1, tooManyWaypoints.str())->GetPath("waypoints"), nullptr);
}
