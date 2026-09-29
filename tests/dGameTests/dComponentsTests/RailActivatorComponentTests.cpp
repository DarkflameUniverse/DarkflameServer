#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDRailActivatorComponent.h"
#include "Entity.h"
#include "MovementMessages.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"
#include "RailActivatorComponent.h"

#include <gtest/gtest.h>

// The rider flags come from the RailActivatorComponent row; a level key, when there, replaces the row's value.
class RailActivatorComponentTests : public GameDependenciesTest {
protected:
	static constexpr int32_t COMPONENT_ID = 9999;
	std::unique_ptr<Entity> entity;

	void SetUp() override {
		SetUpDependencies();
		CDRailActivatorComponent row{};
		row.id = COMPONENT_ID;
		row.damageImmune = true;
		row.noAggro = true;
		row.showNameBillboard = true;
		CDClientManager::GetEntriesMutable<CDRailActivatorComponentTable>().push_back(row);

		// The entity's own LOT has no components, so it doesn't look anything up in a database
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
	}

	void TearDown() override {
		entity.reset();
		CDClientManager::GetEntriesMutable<CDRailActivatorComponentTable>().clear();
		TearDownDependencies();
	}
};

TEST_F(RailActivatorComponentTests, WithoutLevelKeysTheRowIsUsed) {
	const auto* const rail = entity->AddComponent<RailActivatorComponent>(COMPONENT_ID);
	EXPECT_TRUE(rail->GetDamageImmune());
	EXPECT_TRUE(rail->GetNoAggro());
	EXPECT_TRUE(rail->GetShowNameBillboard());
}

TEST_F(RailActivatorComponentTests, LevelKeysReplaceTheRow) {
	entity->SetVar<bool>(u"rail_activator_damage_immune", false);
	entity->SetVar<bool>(u"rail_no_aggro", false);
	entity->SetVar<bool>(u"rail_show_name_billboard", false);
	const auto* const rail = entity->AddComponent<RailActivatorComponent>(COMPONENT_ID);
	EXPECT_FALSE(rail->GetDamageImmune());
	EXPECT_FALSE(rail->GetNoAggro());
	EXPECT_FALSE(rail->GetShowNameBillboard());
}

TEST_F(RailActivatorComponentTests, NoRowAndNoKeysIsNotImmune) {
	const auto* const rail = entity->AddComponent<RailActivatorComponent>(COMPONENT_ID + 1);
	EXPECT_FALSE(rail->GetDamageImmune());
	EXPECT_FALSE(rail->GetNoAggro());
}

// Live answered the client's RequestRailActivatorState (1479, no payload; 152 packets) with
// NotifyRailActivatorStateChange (1478) to that client: every one of the 413 live packets was bActive = true (80).
TEST_F(RailActivatorComponentTests, RequestRailActivatorStateIsAnswered) {
	using namespace GameMessageTestUtils;
	entity->AddComponent<RailActivatorComponent>(COMPONENT_ID);
	GameMessages::RequestRailActivatorState request;
	const auto packets = Capture([&] { request.Handle(*entity, ClientAddress()); });
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(packets[0].sysAddr, ClientAddress());
	EXPECT_FALSE(packets[0].broadcast);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 00 c6 05 80", 145), FromCapture(packets[0]));
}

TEST_F(RailActivatorComponentTests, InactiveRailIsReportedInactive) {
	using namespace GameMessageTestUtils;
	entity->SetVar<bool>(u"rail_activator_active", false);
	const auto* const rail = entity->AddComponent<RailActivatorComponent>(COMPONENT_ID);
	EXPECT_FALSE(rail->GetActive());
	GameMessages::NotifyRailActivatorStateChange notify;
	notify.bActive = false;
	EXPECT_FALSE(RoundTrip(notify).bActive);
}
