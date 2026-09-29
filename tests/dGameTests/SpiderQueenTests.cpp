#include <gtest/gtest.h>

#include "GameDependencies.h"
#include "Entity.h"
#include "ObjectMessages.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "02_server/Enemy/AG/BossSpiderQueenEnemyServer.h"
#include "02_server/Map/Property/AG_Small/ZoneAgProperty.h"

// The AG property's Spider Queen fight (L_ZONE_AG_PROPERTY.lua, L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua).

using namespace GameMessageTestUtils;

class SpiderQueenTest : public GameDependenciesTest {
protected:
	static constexpr LWOOBJID ZONE = 70368744177662;
	static constexpr LWOOBJID BOSS = 0x1000;
	static constexpr LWOOBJID LAND_TARGET = 0x1001;
	static constexpr LWOOBJID SCREAM_EMITTER = 0x1002;
	static constexpr LWOOBJID SPIDERLING = 0x1003;

	std::vector<std::unique_ptr<Entity>> entities;
	Entity* zone = nullptr;
	Entity* boss = nullptr;
	ZoneAgProperty zoneScript;
	BossSpiderQueenEnemyServer bossScript;

	void SetUp() override {
		SetUpDependencies();
		zone = Add(ZONE, 0, {});
		boss = Add(BOSS, 14381, { "SpiderBoss" });
		zoneScript.SetGameVariables(zone);
	}

	void TearDown() override {
		for (const auto& entity : entities) Game::entityManager->_removeEntity(entity->GetObjectID());
		entities.clear();
		TearDownDependencies();
	}

	Entity* Add(const LWOOBJID id, const LOT lot, const std::vector<std::string>& groups) {
		info.lot = lot;
		auto* const entity = entities.emplace_back(std::make_unique<Entity>(id, info)).get();
		entity->GetGroups() = groups;
		Game::entityManager->_addEntity(entity);
		return entity;
	}
};

TEST_F(SpiderQueenTest, ZoneHandsTheBossItsLandingTargetAndScreamEmitter) {
	Add(LAND_TARGET, 14382, { "Land_Target" });
	Add(SCREAM_EMITTER, 14383, { "Spider_Scream" });

	zoneScript.BaseOnFireEventServerSide(zone, boss, "RetrieveZoneData");

	EXPECT_EQ(zone->GetVar<LWOOBJID>(u"SpiderBossID"), BOSS);
	EXPECT_EQ(boss->GetVar<LWOOBJID>(u"LandingTarget"), LAND_TARGET);
	EXPECT_EQ(boss->GetVar<LWOOBJID>(u"ScreamEmitter"), SCREAM_EMITTER);
	zone->Update(0.0f);
	EXPECT_FALSE(zone->HasTimer("ProcessGroupObj_LandingTarget"));
	EXPECT_FALSE(zone->HasTimer("ProcessGroupObj_ScreamEmitter"));
}

TEST_F(SpiderQueenTest, ZoneLooksAgainWhileTheObjectsAreNotSpawned) {
	zoneScript.BaseOnFireEventServerSide(zone, boss, "RetrieveZoneData");

	EXPECT_EQ(boss->GetVar<LWOOBJID>(u"LandingTarget"), LWOOBJID_EMPTY);
	zone->Update(0.0f); // starts the pending timers
	EXPECT_TRUE(zone->HasTimer("ProcessGroupObj_LandingTarget"));
	EXPECT_TRUE(zone->HasTimer("ProcessGroupObj_ScreamEmitter"));

	Add(SCREAM_EMITTER, 14383, { "Spider_Scream" });
	zoneScript.OnTimerDone(zone, "ProcessGroupObj_ScreamEmitter");
	EXPECT_EQ(boss->GetVar<LWOOBJID>(u"ScreamEmitter"), SCREAM_EMITTER);
}

TEST_F(SpiderQueenTest, ASpiderlingDeathMakesTheBossScreamFromTheMountain) {
	boss->SetVar<LWOOBJID>(u"ScreamEmitter", SCREAM_EMITTER);
	auto* const spiderling = Add(SPIDERLING, 16197, { "BabySpider" });

	const auto packets = Capture([&] { bossScript.OnNotifyObject(boss, spiderling, "SpiderlingDied", 0, 0); });
	const auto sent = SentGameMessages<GameMessages::NotifyClientObject>(packets);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].target, BOSS);
	EXPECT_EQ(sent[0].name, u"EmitScream");
	EXPECT_EQ(sent[0].paramObj, SCREAM_EMITTER);
	EXPECT_TRUE(packets[0].broadcast);
}

TEST_F(SpiderQueenTest, OnlySpiderlingsMakeTheBossScream) {
	auto* const other = Add(SPIDERLING, 16196, {});
	const auto packets = Capture([&] { bossScript.OnNotifyObject(boss, other, "SpiderlingDied", 0, 0); });
	EXPECT_TRUE(packets.empty());
}

TEST_F(SpiderQueenTest, TheMeleeSmashLocksTheSpecialsForItsLength) {
	bossScript.OnSkillCast(boss, 303);
	EXPECT_FALSE(boss->GetBoolean(u"bSpecialLock"));

	bossScript.OnSkillCast(boss, 322);
	EXPECT_TRUE(boss->GetBoolean(u"bSpecialLock"));
	boss->Update(0.0f);
	EXPECT_TRUE(boss->HasTimer("UnlockSpecials"));

	// A special due during the smash waits for it
	bossScript.SpiderSkillManager(boss, true);
	EXPECT_TRUE(boss->GetBoolean(u"bSpecialQueued"));

	bossScript.OnTimerDone(boss, "UnlockSpecials");
	EXPECT_FALSE(boss->GetBoolean(u"bSpecialLock"));
	EXPECT_FALSE(boss->GetBoolean(u"bSpecialQueued"));
}

namespace {
	// The arena's 16 rapid fire targets as the AG property map places them: target n has CWOrder n and CWOrder2
	// (n + 8) mod 16 (1 to 16); ZoneNTargets holds the targets 2N-1, 2N and 2N+1 (target 1 again after 16), so
	// neighbouring groups share their edge target. The object ID is 100 + CWOrder.
	std::vector<BossSpiderQueenEnemyServer::RapidFireTarget> ArenaGroup(const std::string& group) {
		const int32_t zone = group[4] - '0';
		std::vector<BossSpiderQueenEnemyServer::RapidFireTarget> targets;
		for (int32_t n = 2 * zone - 1; n <= 2 * zone + 1; n++) {
			const int32_t cwOrder = (n - 1) % 16 + 1;
			targets.push_back({ 100 + cwOrder, cwOrder, (cwOrder + 7) % 16 + 1 });
		}
		// The sweep sorts them; hand them over out of order
		std::ranges::reverse(targets);
		return targets;
	}

	std::vector<LWOOBJID> Ids(const std::vector<int32_t>& cwOrders) {
		std::vector<LWOOBJID> ids;
		for (const auto order : cwOrders) ids.push_back(100 + order);
		return ids;
	}
}

TEST_F(SpiderQueenTest, RapidFireSweepsTheThreeZonesAroundThePlayerClockwise) {
	EXPECT_EQ(BossSpiderQueenEnemyServer::BuildRapidFireTargets("Zone5Vol", true, ArenaGroup), Ids({ 7, 8, 9, 10, 11, 12, 13 }));
}

TEST_F(SpiderQueenTest, RapidFireSweepsCounterClockwise) {
	EXPECT_EQ(BossSpiderQueenEnemyServer::BuildRapidFireTargets("Zone5Vol", false, ArenaGroup), Ids({ 13, 12, 11, 10, 9, 8, 7 }));
}

TEST_F(SpiderQueenTest, RapidFireSweepsOverTheEdgeBetweenZonesEightAndOne) {
	EXPECT_EQ(BossSpiderQueenEnemyServer::BuildRapidFireTargets("Zone1Vol", true, ArenaGroup), Ids({ 15, 16, 1, 2, 3, 4, 5 }));
	EXPECT_EQ(BossSpiderQueenEnemyServer::BuildRapidFireTargets("Zone8Vol", false, ArenaGroup), Ids({ 3, 2, 1, 16, 15, 14, 13 }));
	EXPECT_EQ(BossSpiderQueenEnemyServer::BuildRapidFireTargets("Zone2Vol", true, ArenaGroup), Ids({ 1, 2, 3, 4, 5, 6, 7 }));
}

TEST_F(SpiderQueenTest, RainOfFireTakesTheFirstGroupAndTwoOfEachOther) {
	const std::vector<std::vector<LWOOBJID>> groups = { { 1, 2, 3 }, { 10, 11, 12, 13 }, { 20 }, {}, { 40, 41 } };
	// Always the last remaining target
	const auto impacts = BossSpiderQueenEnemyServer::PickRainOfFireImpacts(groups, [](const size_t count) { return count - 1; });
	EXPECT_EQ(impacts, (std::vector<LWOOBJID>{ 1, 2, 3, 13, 12, 20, 41, 40 }));
}

TEST_F(SpiderQueenTest, ZoneVolumesTellTheBossWhereThePlayerIs) {
	for (const auto& group : { "Zone1Vol", "Zone2Vol", "Zone3Vol", "Zone4Vol", "Zone5Vol", "Zone6Vol", "Zone7Vol", "Zone8Vol", "AggroVol", "TeleVol" }) {
		Add(0x2000 + static_cast<LWOOBJID>(entities.size()), 14400, { group });
	}
	zone->SetVar<LWOOBJID>(u"SpiderBossID", BOSS);
	zoneScript.ProcessZoneVolumes(zone);
	zoneScript.ProcessZoneVolumes(zone); // asked again: each volume reports once
	for (const auto& entity : entities) {
		if (entity->GetGroups().size() == 1 && entity->GetGroups()[0].ends_with("Vol")) {
			EXPECT_TRUE(entity->GetVar<bool>(u"spiderBossSensor"));
		}
	}
}

TEST_F(SpiderQueenTest, ZoneVolumesWaitForTheVolumesToSpawn) {
	zoneScript.ProcessZoneVolumes(zone);
	zone->Update(0.0f);
	EXPECT_TRUE(zone->HasTimer("ProcessGroupObj_ZoneVolumes"));
}

TEST_F(SpiderQueenTest, NoSpecialsBeforeTheFirstWave) {
	// Stage 1 has no special attack; the skill manager does nothing
	const auto packets = Capture([&] { bossScript.SpiderSkillManager(boss, true); });
	EXPECT_TRUE(packets.empty());
	EXPECT_FALSE(boss->GetBoolean(u"isSpecialAttacking"));
}
