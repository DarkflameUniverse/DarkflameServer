// Client -> server game messages DLU used to drop (docs/CaptureUnknowns.md section 4). Packets are from 2011/2012
// live captures with the object ID replaced.
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"

#include "CharacterComponent.h"
#include "Entity.h"
#include "PlayerMessages.h"

#include <memory>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	// A synthetic player object ID, as the object ID of the live packets below
	constexpr const char* PLAYER_HEADER = "53040005000000000100000000000010";
}

class ClientMessagesTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> player;
	std::unique_ptr<Character> character;

	void SetUp() override {
		SetUpDependencies();
		player = std::make_unique<Entity>(0x1000000000000001LL, info);
		character = std::make_unique<Character>(1, nullptr);
		player->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void TearDown() override {
		player.reset();
		character.reset();
		TearDownDependencies();
	}
};

// SetTooltipFlag (469): bFlag, then the tooltip. Both live samples flagged tooltip 24; live saved ttip="16777216".
TEST_F(ClientMessagesTests, SetTooltipFlagMatchesLiveCapture) {
	auto msg = FromLiveClientCapture<GameMessages::SetTooltipFlag>(std::string(PLAYER_HEADER) + "d501" + "8c00000000");
	EXPECT_TRUE(msg.bFlag);
	EXPECT_EQ(msg.iToolTip, 24);
	EXPECT_EQ(RoundTrip(msg).iToolTip, 24);
	ExpectTruncatedFails(msg);

	auto* const characterComponent = player->GetComponent<CharacterComponent>();
	msg.Handle(*player, UNASSIGNED_SYSTEM_ADDRESS);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 16777216u);
}

// The client's bit operations: set shifts 1 by the tooltip; clear masks with ~1 shifted by the tooltip (clearing
// every lower bit too); shifts of 64 or more give 0; tooltips above 127 are ignored.
TEST_F(ClientMessagesTests, SetTooltipFlagFollowsTheClient) {
	auto* const characterComponent = player->GetComponent<CharacterComponent>();
	characterComponent->SetTooltipFlag(0, true);
	characterComponent->SetTooltipFlag(3, true);
	characterComponent->SetTooltipFlag(63, true);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0x8000000000000009ULL);
	characterComponent->SetTooltipFlag(64, true);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0x8000000000000009ULL);
	characterComponent->SetTooltipFlag(128, false);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0x8000000000000009ULL);
	characterComponent->SetTooltipFlag(3, false);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0x8000000000000000ULL);
	characterComponent->SetTooltipFlag(100, false);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0u);
}
