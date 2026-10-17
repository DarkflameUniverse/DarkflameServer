#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "Entity.h"
#include "InventoryComponent.h"

#include <gtest/gtest.h>

class InventoryGroupTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};

	void SetUp() override {
		SetUpDependencies();
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	void Update(const InventoryComponent::GroupUpdateCommand command, const std::string& id, const eInventoryType type, const LOT lot = LOT_NULL) {
		InventoryComponent::GroupUpdate update{};
		update.groupId = id;
		update.groupName = "name";
		update.lot = lot;
		update.inventory = type;
		update.command = command;
		inventory->UpdateGroup(update);
	}
};

// The client's LWOInventoryComponent_Common::msgUpdateInventoryGroup (0x00c955b0) and msgUpdateInventoryGroupContents
// (0x00ce1f50) map the model vault to the models inventory (m_VaultToPlayerInventory)
TEST_F(InventoryGroupTests, TheModelVaultUsesTheModelsGroups) {
	Update(InventoryComponent::GroupUpdateCommand::ADD, "user_group1", eInventoryType::VAULT_MODELS);
	Update(InventoryComponent::GroupUpdateCommand::ADD_LOT, "user_group1", eInventoryType::VAULT_MODELS, 3001);

	const auto& groups = inventory->GetGroups();
	ASSERT_EQ(groups.at(eInventoryType::MODELS).size(), 1u);
	EXPECT_EQ(groups.at(eInventoryType::MODELS)[0].lots.size(), 1u);
	EXPECT_FALSE(groups.contains(eInventoryType::VAULT_MODELS));
}

TEST_F(InventoryGroupTests, OnlyBricksAndModelsHaveGroups) {
	Update(InventoryComponent::GroupUpdateCommand::ADD, "user_group1", eInventoryType::ITEMS);
	EXPECT_FALSE(inventory->GetGroups().contains(eInventoryType::ITEMS));

	Update(InventoryComponent::GroupUpdateCommand::ADD, "user_group1", eInventoryType::BRICKS);
	EXPECT_EQ(inventory->GetGroups().at(eInventoryType::BRICKS).size(), 1u);
}
