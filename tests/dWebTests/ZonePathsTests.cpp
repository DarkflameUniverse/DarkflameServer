#include <gtest/gtest.h>

#include <cstring>

#include "ZonePaths.h"

namespace {
	// Builds a small .luz the way the client's files are laid out (little-endian)
	struct LuzWriter {
		std::string data;
		template<typename T> LuzWriter& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		LuzWriter& Text(const std::string& text) { Put<uint8_t>(static_cast<uint8_t>(text.size())); data += text; return *this; }
		LuzWriter& Wide(const std::string& text) { Put<uint8_t>(static_cast<uint8_t>(text.size())); for (char c : text) Put<uint16_t>(static_cast<uint16_t>(c)); return *this; }
		LuzWriter& Point(float x, float y, float z) { return Put(x).Put(y).Put(z); }
	};

	std::string SampleZone() {
		LuzWriter w;
		w.Put<uint32_t>(41).Put<uint32_t>(3).Put<uint32_t>(1150); // version, revision, world
		w.Point(1, 2, 3).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f);  // spawn point and rotation
		w.Put<uint32_t>(1);                                        // one scene
		w.Text("scene.lvl").Put<uint32_t>(1).Put<uint32_t>(0).Text("Global").Put<uint8_t>(1).Put<uint8_t>(2).Put<uint8_t>(3);
		w.Put<uint8_t>(0).Text("zone.raw").Text("Name").Text("Description"); // no zone boundaries
		w.Put<uint32_t>(1);                                        // one transition: 2 points in this version
		for (int i = 0; i < 2; i++) w.Put<uint64_t>(1).Point(0, 0, 0);
		const auto pathsAt = w.data.size();
		w.Put<uint32_t>(0).Put<uint32_t>(1).Put<uint32_t>(3);      // path data length (below), chunk version, 3 paths

		// A movement path: one waypoint with a command
		w.Put<uint32_t>(18).Wide("Patrol").Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<uint32_t>(1).Point(5, 0, 5).Put<uint32_t>(1).Wide("delay").Wide("2");

		// A spawner path: rotation and config per waypoint
		w.Put<uint32_t>(18).Wide("Spawner").Put<uint32_t>(4).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(6010).Put<uint32_t>(10).Put<int32_t>(1).Put<uint32_t>(1).Put<int64_t>(123).Put<uint8_t>(1);
		w.Put<uint32_t>(1).Point(1, 1, 1).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put<uint32_t>(0);

		// The property path
		w.Put<uint32_t>(8).Wide("PropertyPath").Put<uint32_t>(2).Put<uint32_t>(0).Put<uint32_t>(0);
		w.Put<int32_t>(0).Put<int32_t>(1000).Put<uint32_t>(0).Put<uint64_t>(1100);
		w.Wide("Block Yard");
		w.Put<uint32_t>(4); for (char c : std::string("Desc")) w.Put<uint16_t>(static_cast<uint16_t>(c));
		w.Put<int32_t>(0).Put<uint32_t>(0).Put(1.0f).Put<uint32_t>(0).Put<uint32_t>(0).Point(0, 0, 0).Put(128.0f);
		w.Put<uint32_t>(3).Point(0, 0, 0).Point(10, 0, 0).Point(10, 0, 10);
		const auto length = static_cast<uint32_t>(w.data.size() - pathsAt - 4);
		std::memcpy(w.data.data() + pathsAt, &length, sizeof(length));
		return w.data;
	}
}

TEST(ZonePathsTests, FindsThePropertyArea) {
	std::string error;
	const auto areas = ZonePaths::ReadPropertyAreas(SampleZone(), error);
	ASSERT_TRUE(areas) << error;
	ASSERT_EQ(areas->size(), 1u);
	const auto& area = areas->front();
	EXPECT_EQ(area.name, "PropertyPath");
	EXPECT_EQ(area.displayName, "Block Yard");
	EXPECT_EQ(area.maxBuildHeight, 128.0f);
	ASSERT_EQ(area.outline.size(), 3u);
	EXPECT_EQ(area.outline[1].x, 10.0f);
	EXPECT_EQ(area.outline[2].z, 10.0f);
}

TEST(ZonePathsTests, CutShortFilesFail) {
	const auto zone = SampleZone();
	for (const size_t length : { size_t{ 3 }, size_t{ 40 }, zone.size() / 2, zone.size() - 1 }) {
		std::string error;
		EXPECT_FALSE(ZonePaths::ReadPropertyAreas(zone.substr(0, length), error)) << length;
		EXPECT_FALSE(error.empty()) << length;
	}
}

TEST(ZonePathsTests, ReadsSceneFiles) {
	EXPECT_EQ(ZonePaths::ReadSceneFiles(SampleZone()), (std::vector<std::string>{ "scene.lvl" }));
	EXPECT_TRUE(ZonePaths::ReadSceneFiles("xx").empty());
}

// The 3D views read only the start of a zone file: the scenes and the terrain file's name, not the paths after them
TEST(ZonePathsTests, ReadsTheHeaderWithoutPaths) {
	std::string error;
	const auto zone = SampleZone();
	const auto header = ZonePaths::ReadHeader(zone, error);
	ASSERT_TRUE(header.has_value()) << error;
	ASSERT_EQ(header->scenes.size(), 1u);
	EXPECT_EQ(header->scenes[0].name, "Global");
	EXPECT_EQ(header->zoneRawPath, "zone.raw");
	EXPECT_EQ(header->spawnpoint, NiPoint3(1, 2, 3));
	EXPECT_TRUE(header->paths.empty());
	EXPECT_TRUE(header->sceneTransitions.empty());

	// Everything after the zone's description can be missing or damaged
	const auto end = zone.find("Description") + std::string("Description").size();
	EXPECT_TRUE(ZonePaths::ReadHeader(zone.substr(0, end), error).has_value());
	EXPECT_FALSE(ZonePaths::ReadHeader(zone.substr(0, end - 1), error).has_value());
	EXPECT_FALSE(ZonePaths::Read(zone.substr(0, end), error).has_value());
}
