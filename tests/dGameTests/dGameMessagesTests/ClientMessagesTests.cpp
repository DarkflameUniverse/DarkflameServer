// Client -> server game messages DLU used to drop (docs/CaptureUnknowns.md section 4). Packets are from 2011/2012
// live captures with the object ID replaced.
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"

#include "CDClientDatabase.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "SkillMessages.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
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

// SetLastCustomBuild (890): a u32-length wide string. 202 live packets, sent when the carried rocket is assembled.
TEST_F(ClientMessagesTests, SetLastCustomBuildMatchesLiveCapture) {
	auto msg = FromLiveClientCapture<GameMessages::SetLastCustomBuild>(std::string(PLAYER_HEADER) + "7a03" +
		"1600000031003a00310034003400350034003b0031003a0034003700310034003b0031003a0034003700310035003b00");
	EXPECT_EQ(msg.tokenizedLOTList, u"1:14454;1:4714;1:4715;");
	EXPECT_EQ(RoundTrip(msg).tokenizedLOTList, msg.tokenizedLOTList);
	ExpectTruncatedFails(msg);

	// Kept as the rocket config (saved as char@lcbp) without turning on the landing
	auto* const characterComponent = player->GetComponent<CharacterComponent>();
	msg.Handle(*player, UNASSIGNED_SYSTEM_ADDRESS);
	EXPECT_EQ(characterComponent->GetLastRocketConfig(), u"1:14454;1:4714;1:4715;");
	EXPECT_FALSE(characterComponent->GetIsLanding());
}

// CasterDead (120): an optional caster, then an optional skill handle. 303 live packets, sent through the targeted
// player when an enemy's skill arrived after the enemy died.
TEST_F(ClientMessagesTests, CasterDeadMatchesLiveCapture) {
	auto msg = FromLiveClientCapture<GameMessages::CasterDead>(std::string(PLAYER_HEADER) + "7800" + "dade8000002000024040000000");
	EXPECT_EQ(msg.i64Caster, 0x40040000000bdb5LL);
	EXPECT_EQ(msg.uiSkillHandle, 1u);
	EXPECT_EQ(RoundTrip(msg).i64Caster, msg.i64Caster);
	ExpectTruncatedFails(msg);
}

// The caster's skill with that handle ends only when the server also sees the caster as dead.
TEST_F(ClientMessagesTests, CasterDeadEndsTheDeadCastersSkill) {
	CDClientDatabase::Connect(":memory:"); // the entity manager looks the new entity up
	CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	auto* const caster = Game::entityManager->CreateEntity(info, nullptr, nullptr, false, 0x40040000000bdb5LL);
	ASSERT_NE(caster, nullptr);
	auto* const destroyable = caster->AddComponent<DestroyableComponent>(-1);
	auto* const skills = caster->AddComponent<SkillComponent>(-1);
	RakNet::BitStream empty;
	skills->CastPlayerSkill(0, 1, empty, player->GetObjectID());
	skills->CastPlayerSkill(0, 2, empty, player->GetObjectID());

	GameMessages::CasterDead msg;
	msg.i64Caster = caster->GetObjectID();
	msg.uiSkillHandle = 1;
	msg.Handle(*player, UNASSIGNED_SYSTEM_ADDRESS);
	EXPECT_TRUE(skills->HasSkill(1)); // alive: a client can't cancel it

	destroyable->SetIsDead(true);
	msg.Handle(*player, UNASSIGNED_SYSTEM_ADDRESS);
	EXPECT_FALSE(skills->HasSkill(1));
	EXPECT_TRUE(skills->HasSkill(2));
}

// ResyncEquipment (1238): no payload (59 live packets). The next serialization carries the equipped items again.
TEST_F(ClientMessagesTests, ResyncEquipmentResendsTheEquipment) {
	auto msg = FromLiveClientCapture<GameMessages::ResyncEquipment>(std::string(PLAYER_HEADER) + "d604");
	CDClientDatabase::Connect(":memory:"); // the inventory looks its component up
	CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	auto* const inventory = player->AddComponent<InventoryComponent>(-1);
	RakNet::BitStream first;
	inventory->Serialize(first, false); // clears the dirty flag it starts with
	RakNet::BitStream clean;
	inventory->Serialize(clean, false);
	bool equipmentSent = true;
	ASSERT_TRUE(clean.Read(equipmentSent));
	EXPECT_FALSE(equipmentSent);

	msg.Handle(*player, UNASSIGNED_SYSTEM_ADDRESS);
	RakNet::BitStream resent;
	inventory->Serialize(resent, false);
	ASSERT_TRUE(resent.Read(equipmentSent));
	EXPECT_TRUE(equipmentSent);
}
