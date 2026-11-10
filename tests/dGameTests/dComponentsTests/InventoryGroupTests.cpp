#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"

#include <gtest/gtest.h>
#include <tinyxml2.h>

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

TEST_F(InventoryGroupTests, NamesAreCutTo64Characters) {
	const std::u16string longName(70, u'a');
	Group("ADD", "user_group1", longName);
	EXPECT_EQ(Groups()[0].groupName, std::string(64, 'a'));

	Group("MODIFY", "user_group1", std::u16string(65, u'b'));
	EXPECT_EQ(Groups()[0].groupName, std::string(64, 'b'));
}

TEST_F(InventoryGroupTests, TheModelVaultUsesTheModelsGroups) {
	Group("ADD", "user_group1", u"models", eInventoryType::VAULT_MODELS);
	Contents("ADD", "user_group1", 3001, eInventoryType::VAULT_MODELS);

	ASSERT_EQ(Groups(eInventoryType::MODELS).size(), 1u);
	EXPECT_EQ(Groups(eInventoryType::MODELS)[0].lots.size(), 1u);
	EXPECT_FALSE(inventory->GetGroups().contains(eInventoryType::VAULT_MODELS));
}

TEST_F(InventoryGroupTests, ClearEmptiesAGroup) {
	Group("ADD", "user_group1", u"bricks");
	Contents("ADD", "user_group1", 3001);
	Contents("ADD", "user_group1", 3002);
	Contents("CLEAR", "user_group1", 0);

	ASSERT_EQ(Groups().size(), 1u);
	EXPECT_TRUE(Groups()[0].lots.empty());
}

TEST_F(InventoryGroupTests, AGroupHoldsAtMost1000Lots) {
	Group("ADD", "user_group1", u"bricks");
	for (LOT lot = 1; lot <= 1001; lot++) Contents("ADD", "user_group1", lot);

	EXPECT_EQ(Groups()[0].lots.size(), 1000u);
	EXPECT_FALSE(Groups()[0].lots.contains(1001));
}

TEST_F(InventoryGroupTests, LockedGroupsHaveNoLimits) {
	for (uint32_t i = 0; i < InventoryComponent::MaximumGroupCount; i++) Group("ADD", "user_group" + std::to_string(i), u"g");
	Group("ADD", "over", u"g");
	EXPECT_EQ(Groups().size(), InventoryComponent::MaximumGroupCount);

	Group("ADD", "locked", u"g", eInventoryType::BRICKS, true);
	ASSERT_EQ(Groups().size(), InventoryComponent::MaximumGroupCount + 1);
	EXPECT_TRUE(Groups().back().locked);
	for (LOT lot = 1; lot <= 1001; lot++) Contents("ADD", "locked", lot);
	EXPECT_EQ(Groups().back().lots.size(), 1001u);
}

TEST_F(InventoryGroupTests, TheLockedFlagIsSavedAndLoaded) {
	Group("ADD", "locked", u"a", eInventoryType::BRICKS, true);
	Group("ADD", "unlocked", u"b");

	tinyxml2::XMLDocument doc;
	doc.Parse("<obj><inv><bag/><items/></inv></obj>");
	inventory->WriteSaveData(doc);
	auto* const grp = doc.FirstChildElement("obj")->FirstChildElement("inv")->FirstChildElement("grps")->FirstChildElement("grp");
	ASSERT_NE(grp, nullptr);
	EXPECT_STREQ(grp->Attribute("u"), "1");
	EXPECT_STREQ(grp->NextSiblingElement("grp")->Attribute("u"), "0");

	// Saves from before have u="0" or no u
	grp->NextSiblingElement("grp")->DeleteAttribute("u");
	auto loaded = std::make_unique<Entity>(2, info);
	auto* const loadedInventory = loaded->AddComponent<InventoryComponent>(-1);
	loadedInventory->LoadXml(doc);
	const auto& groups = loadedInventory->GetGroups().at(eInventoryType::BRICKS);
	ASSERT_EQ(groups.size(), 2u);
	EXPECT_TRUE(groups[0].locked);
	EXPECT_FALSE(groups[1].locked);
}

TEST_F(InventoryGroupTests, OnlyBricksAndModelsHaveGroups) {
	Group("ADD", "user_group1", u"g", eInventoryType::ITEMS);
	EXPECT_FALSE(inventory->GetGroups().contains(eInventoryType::ITEMS));

	Group("ADD", "user_group1", u"g", eInventoryType::BRICKS);
	EXPECT_EQ(inventory->GetGroups().at(eInventoryType::BRICKS).size(), 1u);
}
