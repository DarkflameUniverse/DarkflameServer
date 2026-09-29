#include "GameDependencies.h"

#include "DestroyableComponent.h"
#include "Entity.h"

#include <gtest/gtest.h>

// How a level object's config changes its DestroyableComponent, as the client's
// LWODestroyableComponent::LoadConfigData (0x00c44cb0) and LoadDataFromTemplate (0x00c9f900) resolve it.
class DestroyableLevelConfigTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;

	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(1, info);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}
};

TEST_F(DestroyableLevelConfigTests, NoSetFactionKeepsTheTemplateFactions) {
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
	entity->SetVar<bool>(u"override_faction", true);
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
}

TEST_F(DestroyableLevelConfigTests, SetFactionWithoutOverrideFactionIsUsed) {
	entity->SetVar<std::string>(u"set_faction", "6 ");
	EXPECT_EQ(DestroyableComponent::GetLevelFactions(*entity), std::vector<int32_t>{ 6 });
}

TEST_F(DestroyableLevelConfigTests, SetFactionWithOverrideFactionTrueIsUsed) {
	entity->SetVar<std::string>(u"set_faction", "4; 6");
	entity->SetVar<bool>(u"override_faction", true);
	EXPECT_EQ(DestroyableComponent::GetLevelFactions(*entity), (std::vector<int32_t>{ 4, 6 }));
}

TEST_F(DestroyableLevelConfigTests, OverrideFactionFalseIgnoresSetFaction) {
	entity->SetVar<std::string>(u"set_faction", "-1");
	entity->SetVar<bool>(u"override_faction", false);
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
}

TEST_F(DestroyableLevelConfigTests, NoLootMatrixKeysKeepTheTemplateLootMatrix) {
	EXPECT_FALSE(DestroyableComponent::GetLevelLootMatrix(*entity));
	entity->SetVar<bool>(u"smashable_loot_matrix_set", true);
	EXPECT_FALSE(DestroyableComponent::GetLevelLootMatrix(*entity));
}

TEST_F(DestroyableLevelConfigTests, LootMatrixWithoutTheSetKeyIsUsed) {
	entity->SetVar<int32_t>(u"smashable_loot_matrix", 748);
	EXPECT_EQ(DestroyableComponent::GetLevelLootMatrix(*entity), 748);
}

TEST_F(DestroyableLevelConfigTests, LootMatrixWithTheSetKeyTrueIsUsed) {
	entity->SetVar<int32_t>(u"smashable_loot_matrix", 352);
	entity->SetVar<bool>(u"smashable_loot_matrix_set", true);
	EXPECT_EQ(DestroyableComponent::GetLevelLootMatrix(*entity), 352);
}

TEST_F(DestroyableLevelConfigTests, LootMatrixWithTheSetKeyFalseIsIgnored) {
	entity->SetVar<int32_t>(u"smashable_loot_matrix", 227);
	entity->SetVar<bool>(u"smashable_loot_matrix_set", false);
	EXPECT_FALSE(DestroyableComponent::GetLevelLootMatrix(*entity));
}

// A luz spawner path's waypoint config reaches the spawned entity: the waypoint's name/value pairs are parsed as
// "name=value" into the spawner node's config, which a spawner copies into the entity's settings. The GF Large Crate
// (LOT 1859, path CrateMast) and the FV small white shrine (LOT 3141, path gate_statue_quickbuild) set matrix 29.
TEST_F(DestroyableLevelConfigTests, SpawnerPathLootMatrixReachesTheEntity) {
	LwoNameValue waypointConfig;
	waypointConfig.ParseInsert(std::string("smashable_loot_matrix") + "=" + "1:29");
	waypointConfig.ParseInsert(std::string("smashable_loot_matrix_set") + "=" + "7:1");

	EntityInfo spawnedInfo = info;
	spawnedInfo.settings = waypointConfig;
	const Entity spawned(2, spawnedInfo);
	EXPECT_EQ(DestroyableComponent::GetLevelLootMatrix(spawned), 29);
}

TEST_F(DestroyableLevelConfigTests, LootMatrixMinusOneIsIgnored) {
	entity->SetVar<int32_t>(u"smashable_loot_matrix", -1);
	EXPECT_FALSE(DestroyableComponent::GetLevelLootMatrix(*entity));
	entity->SetVar<bool>(u"smashable_loot_matrix_set", true);
	EXPECT_FALSE(DestroyableComponent::GetLevelLootMatrix(*entity));
}
