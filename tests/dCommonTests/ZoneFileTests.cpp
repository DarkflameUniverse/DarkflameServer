#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <sstream>

#include "Game.h"
#include "GeneralUtils.h"
#include "LevelFile.h"
#include "Logger.h"
#include "ZoneFile.h"

namespace {
	// Little-endian file bytes, as the client's files are laid out
	struct ZoneBytes {
		std::string data;
		template<typename T> ZoneBytes& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		template<typename T> void At(size_t pos, T value) { std::memcpy(data.data() + pos, &value, sizeof(T)); }
		ZoneBytes& Text(const std::string& text) { Put<uint8_t>(static_cast<uint8_t>(text.size())); data += text; return *this; }
		ZoneBytes& Wide(const std::string& text) { Put<uint8_t>(static_cast<uint8_t>(text.size())); for (char c : text) Put<uint16_t>(static_cast<uint16_t>(c)); return *this; }
		ZoneBytes& Point(float x, float y, float z) { return Put(x).Put(y).Put(z); }
		// The path chunk's length, filled in by Done (the path chunk ends the file)
		size_t pathsAt = std::string::npos;
		ZoneBytes& Paths() { pathsAt = data.size(); return Put<uint32_t>(0); }
		const std::string& Done() {
			if (pathsAt != std::string::npos) At<uint32_t>(pathsAt, static_cast<uint32_t>(data.size() - pathsAt - 4));
			return data;
		}
		void Object(uint32_t version, uint32_t lot, float x, const std::string& settings) {
			Put<int64_t>(lot * 10).Put<int32_t>(static_cast<int32_t>(lot));
			if (version >= 38) Put<int32_t>(0); // node type
			if (version >= 32) Put<uint32_t>(0); // glom ID
			Point(x, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f); // position, rotation, scale
			Put<uint32_t>(static_cast<uint32_t>(settings.size()));
			for (char c : settings) Put<uint16_t>(static_cast<uint16_t>(c));
			Put<uint32_t>(0);
		}
	};

	std::string SampleZone() {
		ZoneBytes w;
		w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150); // version, revision, world
		w.Point(1, 2, 3).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);  // spawn point and rotation
		w.Put<uint32_t>(1);
		w.Text("scene.lvl").Put<uint32_t>(7).Put<uint32_t>(0).Text("Global").Put<uint8_t>(1).Put<uint8_t>(2).Put<uint8_t>(3);
		w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description"); // no zone boundaries
		w.Put<uint32_t>(1);
		for (int i = 0; i < 2; i++) w.Put<uint64_t>(1).Point(0, 0, 0);
		w.Paths().Put<uint32_t>(1).Put<uint32_t>(3);
		// Movement: its config is waypoint commands (spaces dropped from the name)
		w.Put<uint32_t>(18).Wide("Patrol").Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<uint32_t>(1).Point(5, 0, 5).Put<uint32_t>(1).Wide("de lay").Wide(" 2 ");
		// Spawner: its config is LDF
		w.Put<uint32_t>(18).Wide("Spawner").Put<uint32_t>(4).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(6010).Put<uint32_t>(10).Put<int32_t>(1).Put<uint32_t>(1).Put<int64_t>(123).Put<uint8_t>(1);
		w.Put<uint32_t>(1).Point(1, 1, 1).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put<uint32_t>(1).Wide("spawner_node_id").Wide("1:4");
		// Property
		w.Put<uint32_t>(8).Wide("PropertyPath").Put<uint32_t>(2).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<int32_t>(1000).Put<uint32_t>(0).Put<uint64_t>(1100);
		w.Wide("Block Yard");
		w.Put<uint32_t>(4); for (char c : std::string("Desc")) w.Put<uint16_t>(static_cast<uint16_t>(c));
		w.Put<int32_t>(0).Put<uint32_t>(0).Put(1.0f).Put<uint32_t>(0).Put<uint32_t>(0).Point(0, 0, 0).Put(128.0f);
		w.Put<uint32_t>(3).Point(0, 0, 0).Point(10, 0, 0).Point(10, 0, 10);
		return w.Done();
	}
}

TEST(ZoneFileTests, ReadsScenesAndPaths) {
	std::istringstream stream(SampleZone());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	EXPECT_EQ(zone.fileFormatVersion, ZoneFile::FileFormatVersion::Latest);
	EXPECT_EQ(zone.worldID, 1150u);
	EXPECT_EQ(zone.spawnpoint, NiPoint3(1, 2, 3));
	ASSERT_EQ(zone.scenes.size(), 1u);
	EXPECT_EQ(zone.scenes[0].filename, "scene.lvl");
	EXPECT_EQ(zone.scenes[0].id, 7u);
	EXPECT_EQ(zone.scenes[0].sceneType, eSceneType::General);
	EXPECT_EQ(zone.zoneRawPath, "zone.raw");
	ASSERT_EQ(zone.sceneTransitions.size(), 1u);
	EXPECT_EQ(zone.sceneTransitions[0].points.size(), 2u);
	ASSERT_EQ(zone.paths.size(), 3u);

	const auto& patrol = zone.paths[0].pathWaypoints.at(0);
	ASSERT_EQ(patrol.commands.size(), 1u);
	EXPECT_EQ(patrol.commands[0].command, eWaypointCommandType::DELAY);
	EXPECT_EQ(patrol.commands[0].data, "2");

	const auto& spawner = zone.paths[1];
	EXPECT_EQ(spawner.spawner.spawnedLOT, 6010);
	EXPECT_EQ(spawner.spawner.spawnerObjID, 123);
	EXPECT_EQ(spawner.pathWaypoints.at(0).config.find(u"spawner_node_id")->second->GetValueAsString(), "4");

	const auto& property = zone.paths[2];
	EXPECT_EQ(property.pathType, PathType::Property);
	EXPECT_EQ(property.property.displayName, "Block Yard");
	EXPECT_EQ(property.property.displayDesc, "Desc");
	EXPECT_EQ(property.property.maxBuildHeight, 128.0f);
	EXPECT_EQ(property.pathWaypoints.size(), 3u);
}

// Boundary lines sit between the scenes and the terrain file's name
TEST(ZoneFileTests, ReadsZoneBoundaries) {
	ZoneBytes w;
	w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150);
	w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
	w.Put<uint32_t>(1);
	w.Text("scene.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(2);
	w.Point(1, 0, 0).Point(10, 20, 30).Put<uint16_t>(1100).Put<uint16_t>(7).Put<uint32_t>(4).Point(5, 6, 7);
	w.Point(0, 0, -1).Point(-1, -2, -3).Put<uint16_t>(1200).Put<uint16_t>(0).Put<uint32_t>(0).Point(0, 0, 0);
	w.Text("zone.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(0); // no transitions, no paths

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.zoneBoundaries.size(), 2u);
	const auto& boundary = zone.zoneBoundaries[0];
	EXPECT_EQ(boundary.normal, NiPoint3(1, 0, 0));
	EXPECT_EQ(boundary.point, NiPoint3(10, 20, 30));
	EXPECT_EQ(boundary.destZoneID, LWOZONEID(1100, 7, 0));
	EXPECT_EQ(boundary.destSceneID, 4u);
	EXPECT_EQ(boundary.spawnLocation, NiPoint3(5, 6, 7));
	EXPECT_EQ(zone.zoneBoundaries[1].destZoneID, LWOZONEID(1200, 0, 0));
	EXPECT_EQ(zone.zoneRawPath, "zone.raw");
	EXPECT_EQ(zone.zoneName, "Name");
	EXPECT_EQ(zone.zoneDesc, "Description");
}

// From LateAlpha (37) on the scene count is a u32 (some LUP zones are 37)
TEST(ZoneFileTests, LateAlphaSceneCountIsAU32) {
	ZoneBytes w;
	w.Put<uint32_t>(37).Put<uint32_t>(1).Put<uint32_t>(20022); // version, revision, world; no spawn point before 38
	w.Put<uint32_t>(1);
	w.Text("scene.lvl").Put<uint32_t>(5).Put<uint32_t>(0).Text("Global Scene").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(0); // no transitions, no paths

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.scenes.size(), 1u);
	EXPECT_EQ(zone.scenes[0].filename, "scene.lvl");
	EXPECT_EQ(zone.scenes[0].id, 5u);
	EXPECT_EQ(zone.zoneRawPath, "zone.raw");
}

// Before version 30 a scene is only its SceneTable ID; below 20 the file reads as 20
TEST(ZoneFileTests, ReadsVersionsBeforePrePreAlpha) {
	ZoneBytes w;
	w.Put<uint32_t>(12).Put<uint32_t>(53).Put<uint8_t>(4); // version, world, scene count
	w.Put<uint32_t>(9).Put<uint32_t>(3).Put<uint32_t>(7).Put<uint32_t>(3); // SceneTable IDs
	w.Put<uint8_t>(0).Text("zone.raw");
	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	EXPECT_EQ(stream.peek(), std::char_traits<char>::eof());
	EXPECT_EQ(zone.fileFormatVersion, ZoneFile::FileFormatVersion::Oldest);
	EXPECT_EQ(zone.zoneRawPath, "zone.raw");
	ASSERT_EQ(zone.scenes.size(), 4u);
	EXPECT_TRUE(zone.scenes[0].filename.empty());

	// In SceneTable ID order, once each; 7 has no row
	zone.ResolveSceneTable([](uint32_t id) -> std::optional<std::string> {
		if (id == 7) return std::nullopt;
		return "scene" + std::to_string(id) + ".lvl";
	});
	ASSERT_EQ(zone.scenes.size(), 2u);
	EXPECT_EQ(zone.scenes[0].id, 0u);
	EXPECT_EQ(zone.scenes[0].filename, "scene3.lvl");
	EXPECT_EQ(zone.scenes[0].name, "scene3.lvl");
	EXPECT_EQ(zone.scenes[1].id, 1u);
	EXPECT_EQ(zone.scenes[1].filename, "scene9.lvl");
}

// Versions 30-32 have no scene IDs: the client numbers the scenes in file order
TEST(ZoneFileTests, EarlyScenesAreNumberedInOrder) {
	ZoneBytes w;
	w.Put<uint32_t>(32).Put<uint32_t>(70).Put<uint8_t>(3).Text("a.lvl").Text("b.lvl").Text("c.lvl");
	w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description").Put<uint32_t>(0);
	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.scenes.size(), 3u);
	EXPECT_EQ(zone.scenes[0].id, 0u);
	EXPECT_EQ(zone.scenes[1].id, 1u);
	EXPECT_EQ(zone.scenes[2].id, 2u);
	EXPECT_EQ(zone.scenes[2].filename, "c.lvl");
}

// The scenes the client loads: by scene ID and layer, a later one replacing an earlier one; none without a General
// layer; before version 41 only the General layer
TEST(ZoneFileTests, KeepsTheScenesTheClientLoads) {
	for (const uint32_t version : { 40u, 41u }) {
		ZoneBytes w;
		w.Put<uint32_t>(version).Put<uint32_t>(1).Put<uint32_t>(1100);
		w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
		w.Put<uint32_t>(5);
		const auto scene = [&w](const std::string& file, uint32_t id, uint32_t layer) {
			w.Text(file).Put<uint32_t>(id).Put<uint32_t>(layer).Text("n").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
		};
		scene("b.lvl", 2, 0);
		scene("b_audio.lvl", 2, 1);
		scene("orphan_audio.lvl", 3, 1);
		scene("a.lvl", 1, 0);
		scene("b2.lvl", 2, 0);
		w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description");
		w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(0);
		std::istringstream stream(w.Done());
		ZoneFile zone;
		zone.Read(stream);
		EXPECT_FALSE(stream.fail());
		std::vector<std::string> files;
		for (const auto& read : zone.scenes) files.push_back(read.filename);
		if (version == 41) EXPECT_EQ(files, (std::vector<std::string>{ "a.lvl", "b2.lvl", "b_audio.lvl" }));
		else EXPECT_EQ(files, (std::vector<std::string>{ "a.lvl", "b2.lvl" }));
	}
}

// A PrePreAlpha (30) file ends at its terrain file's name: no zone name, description, transitions or paths
TEST(ZoneFileTests, PrePreAlphaHasNoZoneName) {
	ZoneBytes w;
	w.Put<uint32_t>(30).Put<uint32_t>(72).Put<uint8_t>(1).Text("scale.lvl"); // version, world, one scene: only its file
	w.Put<uint8_t>(0).Text("scale.raw");
	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.scenes.size(), 1u);
	EXPECT_EQ(zone.scenes[0].filename, "scale.lvl");
	EXPECT_EQ(zone.zoneRawPath, "scale.raw");
	EXPECT_TRUE(zone.zoneName.empty());
	EXPECT_TRUE(zone.zoneDesc.empty());
}

// Before path version 3 the type is a name and every waypoint has a platform's data and name/value pairs
TEST(ZoneFileTests, ReadsLegacyPaths) {
	ZoneBytes w;
	w.Put<uint32_t>(35).Put<uint32_t>(137).Put<uint8_t>(1); // version, world, scene count
	w.Text("lup.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global Scene").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(0).Text("lup.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0); // no transitions
	w.Paths().Put<uint32_t>(1).Put<uint32_t>(2);
	w.Put<uint32_t>(2).Wide("LavaPath").Wide("npc").Put<uint32_t>(1).Put<uint32_t>(0);
	w.Put<uint32_t>(1).Point(1, 2, 3).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put<uint8_t>(0).Put(3.0f).Put(0.5f);
	w.Put<uint32_t>(1).Wide("delay").Wide("2");
	w.Put<uint32_t>(2).Wide("Mower").Wide("platform").Put<uint32_t>(0).Put<uint32_t>(2);
	w.Put<uint32_t>(1).Point(4, 5, 6).Put(0.0f).Put(1.0f).Put(0.0f).Put(0.0f).Put<uint8_t>(1).Put(7.0f).Put(1.5f).Put<uint32_t>(0);

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	EXPECT_EQ(stream.peek(), std::char_traits<char>::eof());
	ASSERT_EQ(zone.paths.size(), 2u);

	const auto& npc = zone.paths[0];
	EXPECT_EQ(npc.pathType, PathType::Movement);
	EXPECT_EQ(npc.flags, 1u);
	ASSERT_EQ(npc.pathWaypoints.size(), 1u);
	EXPECT_EQ(npc.pathWaypoints[0].position, NiPoint3(1, 2, 3));
	EXPECT_EQ(npc.pathWaypoints[0].speed, 3.0f);
	ASSERT_EQ(npc.pathWaypoints[0].commands.size(), 1u);
	EXPECT_EQ(npc.pathWaypoints[0].commands[0].command, eWaypointCommandType::DELAY);

	const auto& platform = zone.paths[1];
	EXPECT_EQ(platform.pathType, PathType::MovingPlatform);
	EXPECT_EQ(platform.pathBehavior, PathBehavior::Once);
	ASSERT_EQ(platform.pathWaypoints.size(), 1u);
	EXPECT_EQ(platform.pathWaypoints[0].rotation.x, 1.0f);
	EXPECT_EQ(platform.pathWaypoints[0].movingPlatform.lockPlayer, 1);
	EXPECT_EQ(platform.pathWaypoints[0].speed, 7.0f);
	EXPECT_EQ(platform.pathWaypoints[0].movingPlatform.wait, 1.5f);
}

// Spawner paths before version 9 have no activate-on-load byte; the client then activates them on load
TEST(ZoneFileTests, SpawnerNetActiveFromVersion9) {
	ZoneBytes w;
	w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150);
	w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
	w.Put<uint32_t>(1);
	w.Text("scene.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(2);
	for (const uint32_t version : { 8u, 9u }) {
		w.Put<uint32_t>(version).Wide("Spawner").Put<uint32_t>(4).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(6010).Put<uint32_t>(10).Put<int32_t>(1).Put<uint32_t>(1).Put<int64_t>(123);
		if (version >= 9) w.Put<uint8_t>(0);
		w.Put<uint32_t>(1).Point(1, 1, 1).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put<uint32_t>(0);
	}

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	EXPECT_EQ(stream.peek(), std::char_traits<char>::eof());
	ASSERT_EQ(zone.paths.size(), 2u);
	EXPECT_EQ(zone.paths[0].spawner.spawnerNetActive, 1);
	EXPECT_EQ(zone.paths[0].pathWaypoints.at(0).position, NiPoint3(1, 1, 1));
	EXPECT_EQ(zone.paths[1].spawner.spawnerNetActive, 0);
}

// Spawner and camera paths have their own fields only from version 4
TEST(ZoneFileTests, SpawnerAndCameraFieldsFromVersion4) {
	ZoneBytes w;
	w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150);
	w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
	w.Put<uint32_t>(1);
	w.Text("scene.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(2);
	w.Put<uint32_t>(3).Wide("Spawner").Put<uint32_t>(4).Put<uint32_t>(0).Put<uint32_t>(0);
	w.Put<uint32_t>(1).Point(1, 1, 1).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put<uint32_t>(0);
	w.Put<uint32_t>(3).Wide("Camera").Put<uint32_t>(3).Put<uint32_t>(0).Put<uint32_t>(0);
	w.Put<uint32_t>(1).Point(2, 2, 2).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f).Put(0.5f).Put(0.0f).Put(0.0f).Put(0.0f);

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.paths.size(), 2u);
	EXPECT_EQ(zone.paths[0].spawner.spawnedLOT, 0);
	EXPECT_EQ(zone.paths[0].pathWaypoints.at(0).position, NiPoint3(1, 1, 1));
	EXPECT_TRUE(zone.paths[1].camera.nextPath.empty());
	EXPECT_EQ(zone.paths[1].pathWaypoints.at(0).position, NiPoint3(2, 2, 2));
}

// Values the client adjusts as it reads them: a reputation multiplier outside 0-1000 is 0, a camera FOV before path
// version 11 is divided by 1.25
TEST(ZoneFileTests, AdjustsValuesAsTheClient) {
	ZoneBytes w;
	w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150);
	w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
	w.Put<uint32_t>(1);
	w.Text("scene.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
	w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description");
	w.Put<uint32_t>(0).Paths().Put<uint32_t>(1).Put<uint32_t>(3);
	for (const float multiplier : { 2000.0f, 3.0f }) {
		w.Put<uint32_t>(8).Wide("Property").Put<uint32_t>(2).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<int32_t>(0).Put<uint32_t>(0).Put<uint64_t>(0).Wide("").Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<uint32_t>(0).Put(multiplier).Put<uint32_t>(0).Put<uint32_t>(0).Point(0, 0, 0).Put(128.0f);
		w.Put<uint32_t>(0);
	}
	w.Put<uint32_t>(10).Wide("Camera").Put<uint32_t>(3).Put<uint32_t>(0).Put<uint32_t>(0).Wide("");
	w.Put<uint32_t>(1).Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f).Put(50.0f).Put(0.0f).Put(0.0f).Put(0.0f);

	std::istringstream stream(w.Done());
	ZoneFile zone;
	zone.Read(stream);
	EXPECT_FALSE(stream.fail());
	ASSERT_EQ(zone.paths.size(), 3u);
	EXPECT_EQ(zone.paths[0].property.repMultiplier, 0.0f);
	EXPECT_EQ(zone.paths[1].property.repMultiplier, 3.0f);
	EXPECT_EQ(zone.paths[2].pathWaypoints.at(0).camera.fov, 40.0f);
}

// The paths are a chunk the client reads on its own: a path it refuses (here version 19) leaves the zone with no paths,
// and the zone file goes on after the chunk
TEST(ZoneFileTests, RefusedPathsLeaveNoPaths) {
	Game::logger = new Logger("./testing.log", false, false); // the reader logs the refusal
	for (const uint32_t refused : { 0u, 1u, 2u, 3u }) {
		ZoneBytes w;
		w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150);
		w.Point(0, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);
		w.Put<uint32_t>(1);
		w.Text("scene.lvl").Put<uint32_t>(0).Put<uint32_t>(0).Text("Global").Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0);
		w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description").Put<uint32_t>(0);
		const auto size = w.data.size();
		w.Put<uint32_t>(0).Put<uint32_t>(refused == 1 ? 2 : 1).Put<uint32_t>(2);
		w.Put<uint32_t>(18).Wide("Good").Put<uint32_t>(2).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<int32_t>(0).Put<uint32_t>(0).Put<uint64_t>(0).Wide("").Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<uint32_t>(0).Put(1.0f).Put<uint32_t>(0).Put<uint32_t>(0).Point(0, 0, 0).Put(128.0f);
		w.Put<uint32_t>(1).Point(1, 2, 3);
		w.Put<uint32_t>(refused == 2 ? 19 : 18).Wide("Patrol").Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<uint32_t>(1).Point(5, 0, 5).Put<uint32_t>(refused == 3 ? 100 : 0);
		for (uint32_t i = 0; refused == 3 && i < 100; i++) w.Wide("delay").Wide("1");
		w.At<uint32_t>(size, static_cast<uint32_t>(w.data.size() - size - 4));

		std::istringstream stream(w.Done());
		ZoneFile zone;
		zone.Read(stream);
		EXPECT_FALSE(stream.fail()) << refused;
		EXPECT_EQ(stream.peek(), std::char_traits<char>::eof()) << refused;
		EXPECT_EQ(zone.paths.size(), refused == 0 ? 2u : 0u) << refused;
	}
	delete Game::logger;
	Game::logger = nullptr;
}

TEST(ZoneFileTests, ShortFilesThrowOrFail) {
	const auto zone = SampleZone();
	for (const size_t length : { size_t{ 3 }, size_t{ 40 }, zone.size() / 2, zone.size() - 1 }) {
		std::istringstream stream(zone.substr(0, length));
		ZoneFile file;
		bool threw = false;
		try { file.Read(stream); } catch (const std::runtime_error&) { threw = true; }
		EXPECT_TRUE(threw || stream.fail()) << length;
	}
}

// Chunked files lay their objects out by the file info chunk's version
TEST(LevelFileTests, ReadsChunkedObjects) {
	ZoneBytes w;
	const auto chunk = [&w](uint32_t id) {
		const auto start = w.data.size();
		w.Put<uint32_t>(0x4B4E4843).Put<uint32_t>(id).Put<uint16_t>(1).Put<uint16_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
		w.At<uint32_t>(start + 16, static_cast<uint32_t>(w.data.size()));
		return start;
	};
	const auto end = [&w](size_t start) { w.At<uint32_t>(start + 12, static_cast<uint32_t>(w.data.size() - start)); };
	const auto info = chunk(LevelFile::FileInfo);
	w.Put<uint32_t>(41).Put<uint32_t>(9).Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
	end(info);
	const auto objects = chunk(LevelFile::SceneObjectData);
	w.At<uint32_t>(info + 20 + 12, static_cast<uint32_t>(objects)); // the file info gives where the objects start
	w.Put<uint32_t>(2);
	w.Object(41, 4945, 10, "respawnname=0:NS_LW_Portal\nloadOnClientOnly=7:1");
	w.Object(41, 176, -4, "spawntemplate=1:6010");
	end(objects);

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	EXPECT_EQ(level.chunkHeaders.at(LevelFile::FileInfo).fileInfo.revision, 9u);
	ASSERT_EQ(level.objects.size(), 2u);
	EXPECT_EQ(level.objects[0].lot, 4945);
	EXPECT_EQ(level.objects[0].position.x, 10.0f);
	EXPECT_EQ(level.objects[0].settings.find(u"respawnname")->second->GetValueAsString(), "NS_LW_Portal");
	EXPECT_EQ(level.objects[1].id, 1760);
	EXPECT_EQ(level.objects[1].settings.find(u"spawntemplate")->second->GetValueAsString(), "6010");
}

// Some live scenes (e.g. Nimbus Station's Gnarled Forest launch pad) are from before chunks
TEST(LevelFileTests, ReadsFilesWithoutChunks) {
	ZoneBytes w;
	w.Put<uint16_t>(30).Put<uint16_t>(0); // version, type; no important byte before 32
	w.data.append(48 + 12, '\0');                          // version 30 settings the world skips
	w.Put<uint32_t>(0);                                    // no skydome
	w.Put<uint32_t>(1);
	w.Object(30, 4945, 3, "respawnname=0:NS_GF");

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	ASSERT_EQ(level.objects.size(), 1u);
	EXPECT_EQ(level.objects[0].position.x, 3.0f);
	EXPECT_EQ(level.objects[0].settings.find(u"respawnname")->second->GetValueAsString(), "NS_GF");
	EXPECT_EQ(level.chunkHeaders.count(LevelFile::SceneObjectData), 1u);
}

// An object's render techniques (from version 7) are skipped; the fields a file has no room for get the client's defaults
TEST(LevelFileTests, ReadsObjectsAsTheClient) {
	ZoneBytes w;
	w.Put<uint16_t>(30).Put<uint16_t>(0);
	w.data.append(48 + 12, '\0');
	w.Put<uint32_t>(0).Put<uint32_t>(2);
	// With two render techniques
	w.Put<int64_t>(10).Put<int32_t>(1).Point(1, 0, 0).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f).Put<uint32_t>(0);
	w.Put<uint32_t>(2);
	w.data.append(64 + 2 * 133, '\x7f');
	w.Object(30, 2, 5, "a=0:b");

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	ASSERT_EQ(level.objects.size(), 2u);
	EXPECT_EQ(level.objects[0].renderTechniqueCount, 2u);
	EXPECT_EQ(level.objects[0].nodeType, 1u);
	EXPECT_EQ(level.objects[0].glomId, 1u);
	EXPECT_EQ(level.objects[1].lot, 2);
	EXPECT_EQ(level.objects[1].position.x, 5.0f);
}

// Editor settings are skipped by their size; a file older than version 3 has no objects for the client
TEST(LevelFileTests, SkipsEditorSettingsBySize) {
	ZoneBytes w;
	w.Put<uint16_t>(37).Put<uint16_t>(0).Put<uint8_t>(0).Put<uint32_t>(4); // version, type, important, revision
	w.data.append(48 + 8 + 12 + 12 + 12 + 16, '\0');                     // lighting
	w.Put<uint32_t>(0);                                                   // skydome
	for (int i = 0; i < 5; i++) w.Put<uint32_t>(0);
	w.Put<uint32_t>(7);
	w.data.append(7, '\x55');                                            // editor settings
	w.Put<uint32_t>(1);
	w.Object(37, 4945, 3, "a=0:b");

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	ASSERT_EQ(level.objects.size(), 1u);
	EXPECT_EQ(level.objects[0].lot, 4945);
	EXPECT_EQ(level.chunkHeaders.begin()->second.fileInfo.revision, 4u);

	ZoneBytes old;
	old.Put<uint16_t>(2).Put<uint16_t>(0);
	old.data.append(48 + 12, '\0');
	old.Put<uint32_t>(0).Put<uint32_t>(1);
	old.Object(2, 1, 0, "a=0:b");
	std::istringstream oldStream(old.Done());
	LevelFile oldLevel;
	oldLevel.Read(oldStream);
	EXPECT_TRUE(oldLevel.objects.empty());
}

// Config the client changes as it loads an object
TEST(LevelFileTests, FixesUpConfigAsTheClient) {
	ZoneBytes w;
	w.Put<uint16_t>(30).Put<uint16_t>(0);
	w.data.append(48 + 12, '\0');
	w.Put<uint32_t>(0).Put<uint32_t>(4);
	w.Object(30, 176, 0, "respawn=3:5000\nsceneIDOverrideEnabled=7:1");
	w.Object(30, 176, 0, "respawn=5:20000");
	w.Object(30, 176, 0, "respawn=3:20\nspawntemplate=1:14\nsubkey=9:123");
	w.Object(30, 176, 0, "spawntemplate=1:14\nblueprintid=9:5\nDisableModelBehaviors=7:0");

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	ASSERT_EQ(level.objects.size(), 4u);
	const auto value = [&level](size_t object, const std::u16string& key) {
		const auto it = level.objects[object].settings.find(key);
		return it == level.objects[object].settings.end() ? std::string("none") : it->second->GetString();
	};
	EXPECT_EQ(value(0, u"respawn"), "respawn=3:5.000000");
	EXPECT_EQ(value(0, u"sceneLayerIDOverride"), "sceneLayerIDOverride=5:0");
	EXPECT_EQ(value(1, u"respawn"), "respawn=3:20.000000");
	EXPECT_EQ(value(2, u"respawn"), "respawn=3:20.000000");
	EXPECT_EQ(value(2, u"blueprintid"), "blueprintid=9:123");
	EXPECT_EQ(value(2, u"DisableModelBehaviors"), "DisableModelBehaviors=7:1");
	EXPECT_EQ(value(2, u"preventRenderWrapping"), "preventRenderWrapping=7:1");
	EXPECT_EQ(value(3, u"blueprintid"), "blueprintid=9:5");
	EXPECT_EQ(value(3, u"DisableModelBehaviors"), "DisableModelBehaviors=7:0");
}

// An object's config splits at commas as well as line breaks; entries without a '=' are left out
TEST(LevelFileTests, SplitsConfigAsTheClient) {
	ZoneBytes w;
	w.Put<uint16_t>(30).Put<uint16_t>(0);
	w.data.append(48 + 12, '\0');
	w.Put<uint32_t>(0).Put<uint32_t>(1);
	w.Object(30, 2, 0, "a=0:x,b=1:4\n\nstray,c=0:y");

	std::istringstream stream(w.Done());
	LevelFile level;
	level.Read(stream);
	ASSERT_EQ(level.objects.size(), 1u);
	const auto& settings = level.objects[0].settings;
	std::vector<std::string> keys;
	for (const auto& [key, value] : settings) keys.push_back(GeneralUtils::UTF16ToWTF8(key));
	std::ranges::sort(keys);
	EXPECT_EQ(keys, (std::vector<std::string>{ "a", "b", "c" }));
}

TEST(LevelFileTests, DamagedFilesKeepWhatWasRead) {
	ZoneBytes w;
	w.Put<uint16_t>(30).Put<uint16_t>(0);
	w.data.append(48 + 12, '\0');
	w.Put<uint32_t>(0).Put<uint32_t>(2);
	w.Object(30, 1, 0, "a=0:b");
	w.Object(30, 2, 0, "c=0:d");
	w.data.resize(w.data.size() - 2); // cut into the last object

	std::istringstream stream(w.Done());
	LevelFile level;
	EXPECT_THROW(level.Read(stream), std::runtime_error);
	EXPECT_EQ(level.objects.size(), 1u);
}
