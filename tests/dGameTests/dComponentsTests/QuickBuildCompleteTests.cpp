#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "Character.h"
#include "CharacterComponent.h"
#include "CDClientDatabase.h"
#include "Entity.h"
#include "PlayerMessages.h"
#include "dZoneManager.h"
#include "QuickBuildComponent.h"
#include "QuickBuildMessages.h"
#include "eQuickBuildState.h"

#include <algorithm>
#include <memory>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	// The quick build spawns its activator through the entity manager, which reads the CDClient: give it an empty one.
	void ConnectEmptyCDClient() {
		if (CDClientDatabase::isConnected) return;
		CDClientDatabase::Connect(":memory:");
		for (const auto* table : {
			"ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER)",
			"ObjectSkills (objectTemplate INTEGER, skillID INTEGER, castOnType INTEGER, AICombatWeight INTEGER)",
			"SkillBehavior (skillID INTEGER, behaviorID INTEGER)",
			"ItemSets (setID INTEGER, itemIDs TEXT)",
			}) {
			CDClientDatabase::ExecuteDML(std::string("CREATE TABLE ") + table + ";");
		}
	}
}

// What a finished quickbuild sends, compared with live (744 successful builds in the live captures).
class QuickBuildCompleteTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		ConnectEmptyCDClient();
		Game::zoneManager->LoadZone(LWOZONEID(1101, 0, 0)); // the zone of the live samples below
		auto playerInfo = info;
		playerInfo.lot = 1;
		player = std::make_unique<Entity>(0x1000000000000001LL, playerInfo);
		Game::entityManager->_addEntity(player.get()); // the quickbuild looks its builder up there
		character = std::make_unique<Character>(1, nullptr);
		player->SetCharacter(character.get());
		character->SetEntity(player.get());
		player->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress());

		auto buildInfo = info;
		buildInfo.lot = 4717;
		build = std::make_unique<Entity>(0x0102030405060708LL, buildInfo);
		quickBuild = build->AddComponent<QuickBuildComponent>(-1);
		quickBuild->SetResetTime(20.0f);
		quickBuild->SetCompleteTime(0.0f);
	}

	// Starts the build and runs updates until it completes (the first update only sets up the imagination drain).
	std::vector<CapturedPacket> Build() {
		quickBuild->OnUse(player.get());
		return Capture([&] {
			for (int i = 0; i < 3 && quickBuild->GetState() != eQuickBuildState::COMPLETED; i++) quickBuild->Update(0.1f);
			});
	}

	void TearDown() override {
		quickBuild = nullptr;
		build.reset();
		Game::entityManager->_removeEntity(player->GetObjectID());
		player->SetCharacter(nullptr);
		player.reset();
		character.reset();
		TearDownDependencies();
	}

	std::unique_ptr<Entity> player;
	std::unique_ptr<Character> character;
	std::unique_ptr<Entity> build;
	QuickBuildComponent* quickBuild{};
};

TEST_F(QuickBuildCompleteTests, SuccessHasNoDuration) {
	const auto sent = Build();
	ASSERT_EQ(quickBuild->GetState(), eQuickBuildState::COMPLETED);
	const auto enables = SentGameMessages<GameMessages::EnableRebuild>(sent);
	ASSERT_EQ(enables.size(), 1u);
	EXPECT_TRUE(enables[0].bSuccess);
	EXPECT_FALSE(enables[0].bFail);
	EXPECT_EQ(enables[0].fDuration, 0.0f);
	EXPECT_EQ(enables[0].user, player->GetObjectID());
}

// Live sends the builder one more QuickBuildsCompleted for this zone after the EnableRebuild. Bytes: a live
// ModifyPlayerZoneStatistic from zone 1101 with the player ID replaced.
TEST_F(QuickBuildCompleteTests, ZoneStatisticFollowsEnableRebuild) {
	const auto sent = Build();
	const auto ids = SentGameMessageIds(sent);
	const auto enable = std::find(ids.begin(), ids.end(), MessageType::Game::ENABLE_REBUILD);
	const auto statistic = std::find(ids.begin(), ids.end(), MessageType::Game::MODIFY_PLAYER_ZONE_STATISTIC);
	ASSERT_NE(statistic, ids.end());
	EXPECT_LT(enable, statistic);

	std::vector<CapturedPacket> statistics;
	for (const auto& packet : sent) {
		if (!SentGameMessages<GameMessages::ModifyPlayerZoneStatistic>({ packet }).empty()) statistics.push_back(packet);
	}
	ASSERT_EQ(statistics.size(), 1u);
	EXPECT_EQ(statistics[0].sysAddr, ClientAddress());
	EXPECT_FALSE(statistics[0].broadcast);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 16 04 0a 00 00 00 28 80 3a 80 34 80 31 80 35 80 21 00 3a 80 34 80 "
		"36 00 32 00 39 80 21 80 37 80 36 80 38 00 36 00 32 80 3a 00 32 80 32 00 40 40 00 00 29 a0 80", 547), FromCapture(statistics[0]));
}

// The same message for a completed achievement (sent from Mission::Complete).
TEST_F(QuickBuildCompleteTests, AchievementZoneStatisticMatchesLive) {
	const auto sent = Capture([&] { player->GetComponent<CharacterComponent>()->SendZoneStatisticIncrement(u"AchievementsCompleted"); });
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 16 04 0a 80 00 00 20 80 31 80 34 00 34 80 32 80 3b 00 32 80 36 80 "
		"32 80 37 00 3a 00 39 80 21 80 37 80 36 80 38 00 36 00 32 80 3a 00 32 80 32 00 40 40 00 00 29 a0 80", 563), FromCapture(sent[0]));
}
