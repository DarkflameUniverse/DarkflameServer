#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "Character.h"
#include "CharacterComponent.h"
#include "CDClientDatabase.h"
#include "Entity.h"
#include "QuickBuildComponent.h"
#include "QuickBuildMessages.h"
#include "eQuickBuildState.h"

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
