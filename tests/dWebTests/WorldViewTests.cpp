#include <gtest/gtest.h>

#include <cstring>

#include "PositionHistory.h"
#include "WorldScene.h"

namespace {
	// Same layout as LevelObjectsTests: a .lvl with a file info chunk and an object chunk (version 41)
	struct LvlWriter {
		std::string data;
		template<typename T> LvlWriter& Put(T value) { data.append(reinterpret_cast<const char*>(&value), sizeof(T)); return *this; }
		template<typename T> void At(size_t pos, T value) { std::memcpy(data.data() + pos, &value, sizeof(T)); }
		size_t Chunk(uint32_t id) {
			const auto start = data.size();
			Put<uint32_t>(0x4B4E4843).Put<uint32_t>(id).Put<uint16_t>(1).Put<uint16_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
			At<uint32_t>(start + 16, static_cast<uint32_t>(data.size()));
			return start;
		}
		void End(size_t chunk) { At<uint32_t>(chunk + 12, static_cast<uint32_t>(data.size() - chunk)); }
		void Object(uint64_t id, uint32_t lot, float x, float y, float z, const std::string& settings) {
			Put<uint64_t>(id).Put<uint32_t>(lot).Put<uint32_t>(0).Put<uint32_t>(0);
			Put(x).Put(y).Put(z).Put(1.0f).Put(0.0f).Put(0.0f).Put(0.0f).Put(2.0f);
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
		w.At<uint32_t>(info + 32, static_cast<uint32_t>(objects)); // the file info chunk says where the objects start, as in real files
		w.Put<uint32_t>(3);
		w.Object(10, 4945, 1, 2, 3, "respawnname=0:NS_Portal\r");
		w.Object(11, 176, -4, 0, 8, "spawntemplate=1:6010\nspawner_name=0:Crates");
		w.Object(12, 1234, 5, 5, 5, "loadOnClientOnly=7:1");
		w.End(objects);
		return w.data;
	}
}

TEST(WorldSceneTests, ReadsEveryObjectWithSpawnersResolved) {
	const auto objects = WorldScene::ReadObjects(SampleLevel());
	ASSERT_EQ(objects.size(), 3u);
	EXPECT_EQ(objects[0].lot, 4945u);
	EXPECT_EQ(objects[0].templateLot, 4945u);
	EXPECT_EQ(objects[0].name, "NS_Portal");
	EXPECT_FALSE(objects[0].spawner);
	EXPECT_FLOAT_EQ(objects[0].z, 3);
	EXPECT_FLOAT_EQ(objects[0].scale, 2);
	EXPECT_TRUE(objects[1].spawner);
	EXPECT_EQ(objects[1].templateLot, 6010u);
	EXPECT_EQ(objects[1].name, "Crates");
	EXPECT_TRUE(objects[2].clientOnly);
	EXPECT_TRUE(objects[2].name.empty());
}

TEST(WorldSceneTests, DamagedFilesKeepWhatWasRead) {
	auto level = SampleLevel();
	level.resize(level.size() - 20);
	EXPECT_EQ(WorldScene::ReadObjects(level).size(), 2u);
	EXPECT_TRUE(WorldScene::ReadObjects("").empty());
}

TEST(WorldSceneTests, ClassifiesByMostTellingComponent) {
	using WorldScene::Classify;
	const auto index = [](eReplicaComponentType type) {
		for (size_t i = 0; i < WorldScene::KINDS.size(); i++) if (WorldScene::KINDS[i] == type) return i;
		return WorldScene::KINDS.size();
	};
	const auto raw = [](eReplicaComponentType type) { return static_cast<uint32_t>(type); };
	// Render only: other
	EXPECT_EQ(Classify({ raw(eReplicaComponentType::RENDER) }), WorldScene::KINDS.size());
	EXPECT_EQ(Classify({}), WorldScene::KINDS.size());
	// The registry's 7 is the destroyable component
	EXPECT_EQ(Classify({ 7 }), index(eReplicaComponentType::DESTROYABLE));
	// An enemy that can be smashed and offers missions is an enemy
	EXPECT_EQ(Classify({ 7, raw(eReplicaComponentType::MISSION_OFFER), raw(eReplicaComponentType::BASE_COMBAT_AI) }), index(eReplicaComponentType::BASE_COMBAT_AI));
	EXPECT_EQ(Classify({ raw(eReplicaComponentType::MISSION_OFFER), raw(eReplicaComponentType::RENDER) }), index(eReplicaComponentType::MISSION_OFFER));
}

TEST(PositionHistoryTests, KeepsFirstMovingAndIdleSamples) {
	PositionHistory::Throttle throttle;
	const LWOOBJID a = 1;
	// First report in a world is kept
	EXPECT_TRUE(throttle.Keep(a, 1000, 1, 0, 0, 0, 100, 5, 30));
	// Moving, but too soon
	EXPECT_FALSE(throttle.Keep(a, 1000, 1, 3, 0, 0, 102, 5, 30));
	// Moving after the interval
	EXPECT_TRUE(throttle.Keep(a, 1000, 1, 6, 0, 0, 105, 5, 30));
	// Standing still: not until the idle interval
	EXPECT_FALSE(throttle.Keep(a, 1000, 1, 6.1f, 0, 0, 120, 5, 30));
	EXPECT_TRUE(throttle.Keep(a, 1000, 1, 6.1f, 0, 0, 135, 5, 30));
	// Changing instance or zone keeps it at once
	EXPECT_TRUE(throttle.Keep(a, 1000, 2, 6.1f, 0, 0, 136, 5, 30));
	EXPECT_TRUE(throttle.Keep(a, 1100, 2, 6.1f, 0, 0, 137, 5, 30));
	// Players are independent
	EXPECT_TRUE(throttle.Keep(2, 1000, 1, 0, 0, 0, 137, 5, 30));
	EXPECT_EQ(throttle.Size(), 2u);
	throttle.Forget(137);
	EXPECT_EQ(throttle.Size(), 2u);
	throttle.Forget(138);
	EXPECT_EQ(throttle.Size(), 0u);
}

TEST(PositionHistoryTests, BucketsThinLongReplays) {
	// An hour at 5 seconds is 720 samples: every one is kept
	EXPECT_EQ(PositionHistory::BucketFor(3600, 5, 3000), 1);
	// A week needs 202-second buckets to stay under 3000 per player
	EXPECT_EQ(PositionHistory::BucketFor(7 * 86400, 5, 3000), 202);
	EXPECT_GE(PositionHistory::BucketFor(1, 5, 0), 1);
}

// Which models the game draws: as ObjectLoader2::LoadRenderComponent, and never objects it doesn't load (carver_only)
TEST(WorldSceneTests, ClientDrawsWhatTheGameDraws) {
	LvlWriter w;
	auto info = w.Chunk(1000);
	w.Put<uint32_t>(41).Put<uint32_t>(1).Put<uint32_t>(0).Put<uint32_t>(0).Put<uint32_t>(0);
	w.End(info);
	auto objects = w.Chunk(2001);
	w.At<uint32_t>(info + 32, static_cast<uint32_t>(objects)); // the file info chunk says where the objects start, as in real files
	w.Put<uint32_t>(6);
	w.Object(1, 4630, 0, 0, 0, "carver_only=7:1\ncreate_physics=7:1");     // an invisible trigger cube
	w.Object(2, 5651, 0, 0, 0, "carver_only=7:0");                         // loaded, so drawn
	w.Object(3, 5651, 0, 0, 0, "renderDisabled=7:0");                      // any value hides it
	w.Object(4, 176, 0, 0, 0, "spawntemplate=1:6010");                     // drawn as what it spawns
	w.Object(5, 176, 0, 0, 0, "spawner_name=0:Nothing");                   // spawns nothing
	w.Object(6, 5937, 0, 0, 0, "");                                        // left out by number
	w.End(objects);
	const auto read = WorldScene::ReadObjects(w.data);
	ASSERT_EQ(read.size(), 6u);
	using WorldScene::eClientDraw;
	EXPECT_TRUE(read[0].carverOnly);
	EXPECT_EQ(WorldScene::ClientDraws(read[0], "Environmental"), eClientDraw::HIDDEN);
	EXPECT_EQ(WorldScene::ClientDraws(read[1], "Environmental"), eClientDraw::DRAWN);
	EXPECT_EQ(WorldScene::ClientDraws(read[1], "BlockingVolume"), eClientDraw::HIDDEN);
	EXPECT_EQ(WorldScene::ClientDraws(read[1], "PrimitiveModels"), eClientDraw::NO_MODEL);
	EXPECT_EQ(WorldScene::ClientDraws(read[2], "Environmental"), eClientDraw::HIDDEN);
	EXPECT_EQ(WorldScene::ClientDraws(read[3], "Smashables"), eClientDraw::DRAWN);
	EXPECT_EQ(WorldScene::ClientDraws(read[4], "Smashables"), eClientDraw::NO_MODEL);
	EXPECT_EQ(WorldScene::ClientDraws(read[5], "Environmental"), eClientDraw::HIDDEN);
}

// A client script that hides its object as soon as it loads (a top-level SetVisible false in onStartup or
// onRenderComponentReady) makes the viewers treat the object as hidden; conditional ones don't
TEST(WorldSceneTests, ClientScriptsThatHideOnLoad) {
	// scripts/02_client/map/general/l_set_invisible.lua
	EXPECT_TRUE(WorldScene::ClientScriptHidesOnLoad("-- comment\nfunction onRenderComponentReady(self,msg)\n\tself:SetVisible{visible = false, fadeTime = 0}\nend"));
	EXPECT_TRUE(WorldScene::ClientScriptHidesOnLoad("function onStartup(self)\r\n    self:SetVisible{ visible = false }\r\nend\r\n"));
	// Only when a condition holds, in another handler, or shown
	EXPECT_FALSE(WorldScene::ClientScriptHidesOnLoad("function onStartup(self)\n\tif x then\n\t\tself:SetVisible{visible = false}\n\tend\nend"));
	EXPECT_FALSE(WorldScene::ClientScriptHidesOnLoad("function onTimerDone(self,msg)\n\tself:SetVisible{visible = false}\nend"));
	EXPECT_FALSE(WorldScene::ClientScriptHidesOnLoad("function onStartup(self)\n\tself:SetVisible{visible = true}\nend"));
	EXPECT_FALSE(WorldScene::ClientScriptHidesOnLoad(""));
}
