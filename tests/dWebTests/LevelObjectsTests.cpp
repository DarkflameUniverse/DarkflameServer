#include <gtest/gtest.h>

#include <cstring>

#include "LevelObjects.h"

namespace {
	struct LvlWriter {
		std::string data;
		template<typename T> LvlWriter& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		template<typename T> void At(size_t pos, T value) { std::memcpy(data.data() + pos, &value, sizeof(T)); }
		// CHNK, id, version, type, size, data start; the size and start are filled in by End
		size_t Chunk(uint32_t id) {
			const auto start = data.size();
			Put<uint32_t>(0x4B4E4843).Put<uint32_t>(id).Put<uint16_t>(1).Put<uint16_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
			At<uint32_t>(start + 16, static_cast<uint32_t>(data.size()));
			return start;
		}
		void End(size_t chunk) { At<uint32_t>(chunk + 12, static_cast<uint32_t>(data.size() - chunk)); }
		void Object(uint32_t lot, float x, float y, float z, const std::string& settings) {
			Put<uint64_t>(1).Put<uint32_t>(lot).Put<uint32_t>(0).Put<uint32_t>(0); // id, lot, node type, glom ID (version 41)
			Put(x).Put(y).Put(z).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(1.0f); // position, rotation, scale
			Put<uint32_t>(static_cast<uint32_t>(settings.size()));
			for (char c : settings) Put<uint16_t>(static_cast<uint16_t>(c));
			Put<uint32_t>(0);
		}
	};

	std::string SampleLevel() {
		LvlWriter w;
		auto info = w.Chunk(1000);
		w.Put<uint32_t>(41).Put<uint32_t>(1).Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
		w.End(info);
		auto objects = w.Chunk(2001);
		w.Put<uint32_t>(4);
		w.Object(4945, 10, 20, 30, "respawnname=0:NS_LW_Portal\ncustom_config_names=0:");
		w.Object(6010, 1, 1, 1, "spawntemplate=1:6010");                                   // not a spawn point
		w.Object(4945, 5, 5, 5, "respawnname=0:ClientOnly\nloadOnClientOnly=7:1");       // the world never loads it
		w.Object(176, -4, 0, 8, "spawntemplate=1:4945\nrespawnname=0:FromSpawner\r");      // spawned objects carry it too
		w.End(objects);
		return w.data;
	}
}

TEST(LevelObjectsTests, ReadsNamedSpawnPoints) {
	const auto points = LevelObjects::ReadSpawnPoints(SampleLevel());
	ASSERT_EQ(points.size(), 2u);
	EXPECT_EQ(points[0].name, "NS_LW_Portal");
	EXPECT_EQ(points[0].lot, 4945u);
	EXPECT_FLOAT_EQ(points[0].x, 10);
	EXPECT_FLOAT_EQ(points[0].z, 30);
	EXPECT_EQ(points[1].name, "FromSpawner");
	EXPECT_FLOAT_EQ(points[1].x, -4);
}

TEST(LevelObjectsTests, StopsOnDamagedFiles) {
	auto level = SampleLevel();
	level.resize(level.size() - 40); // cut into the last object
	EXPECT_EQ(LevelObjects::ReadSpawnPoints(level).size(), 1u);
	EXPECT_TRUE(LevelObjects::ReadSpawnPoints("").empty());
	EXPECT_TRUE(LevelObjects::ReadSpawnPoints(std::string(64, 'x')).empty());
}
