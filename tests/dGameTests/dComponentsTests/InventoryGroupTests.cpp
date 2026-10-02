#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"

#include <gtest/gtest.h>

// Inventory groups as LWOInventoryComponent_Common::msgUpdateInventoryGroup (0x00c955b0) and
// msgUpdateInventoryGroupContents (0x00ce1f50) keep them.
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

	void Group(const std::string& action, const std::string& id, const std::u16string& name, const eInventoryType type = eInventoryType::BRICKS, const bool locked = false) {
		GameMessages::UpdateInventoryGroup msg;
		msg.action = action;
		msg.groupID = id;
		msg.groupName = name;
		msg.inventoryType = type;
		msg.locked = locked;
		inventory->OnUpdateInventoryGroup(msg);
	}

	void Contents(const std::string& action, const std::string& id, const LOT lot, const eInventoryType type = eInventoryType::BRICKS) {
		GameMessages::UpdateInventoryGroupContents msg;
		msg.action = action;
		msg.groupID = id;
		msg.lot = lot;
		msg.inventoryType = type;
		inventory->OnUpdateInventoryGroupContents(msg);
	}

	const std::vector<InventoryComponent::Group>& Groups(const eInventoryType type = eInventoryType::BRICKS) {
		return inventory->GetGroups().at(type);
	}
};

TEST_F(InventoryGroupTests, AddingAGroupThatExistsDoesNothing) {
	Group("ADD", "user_group1", u"first");
	Contents("ADD", "user_group1", 3001);
	Group("ADD", "user_group1", u"second");

	ASSERT_EQ(Groups().size(), 1u);
	EXPECT_EQ(Groups()[0].groupName, "first");
	EXPECT_EQ(Groups()[0].lots.size(), 1u);
}
