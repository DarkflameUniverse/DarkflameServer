#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDRailActivatorComponent.h"
#include "Entity.h"
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
