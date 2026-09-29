#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "eReplicaComponentType.h"
#include "tinyxml2.h"

#include <gtest/gtest.h>

// What the character save writes as equipped ("eq"), in and out of a pushed equipment state (build mode).
class InventorySaveTests : public GameDependenciesTest {
protected:
	static constexpr LOT HAT = 1000;
	static constexpr LOT MODEL = 6662; // a brick built model item, carried while building on a property

	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};
	tinyxml2::XMLDocument doc;
	LWOOBJID nextId = 0x7000;

	void SetUp() override {
		SetUpDependencies();
		RegisterLot(HAT, "hair");
		RegisterLot(MODEL, "Extra_1");

		// The entity's own LOT has no components, so InventoryComponent doesn't look it up in a database
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);

		doc.Parse("<obj><inv><bag/><items/></inv></obj>");
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot, const std::string& equipLocation) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.equipLocation = equipLocation;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	// The inventory owns the item; equipped the way loading a save equips it
	Item* GiveEquipped(const LOT lot, const eInventoryType type) {
		auto* const bag = inventory->GetInventory(type);
		auto* const item = new Item(nextId++, lot, bag, static_cast<uint32_t>(bag->GetItems().size()), 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
		inventory->UpdateSlot(item->GetInfo().equipLocation, { item->GetId(), item->GetLot(), item->GetCount(), item->GetSlot() });
		return item;
	}

	// The "eq" attribute the save wrote for an item
	bool SavedEquipped(const LWOOBJID id) {
		inventory->UpdateXml(doc);
		for (auto* bag = doc.FirstChildElement("obj")->FirstChildElement("inv")->FirstChildElement("items")->FirstChildElement("in"); bag; bag = bag->NextSiblingElement("in")) {
			for (auto* element = bag->FirstChildElement("i"); element; element = element->NextSiblingElement("i")) {
				if (element->Int64Attribute("id") == id) return element->BoolAttribute("eq");
			}
		}
		ADD_FAILURE() << "item " << id << " was not saved";
		return false;
	}
};

TEST_F(InventorySaveTests, EquippedItemsAreSavedEquipped) {
	const auto* const hat = GiveEquipped(HAT, eInventoryType::ITEMS);
	EXPECT_TRUE(SavedEquipped(hat->GetId()));
}

TEST_F(InventorySaveTests, ModelCarriedInBuildModeIsNotSavedEquipped) {
	const auto* const hat = GiveEquipped(HAT, eInventoryType::ITEMS);
	inventory->PushEquippedItems(); // entering build mode
	const auto* const model = GiveEquipped(MODEL, eInventoryType::MODELS); // picked up off the property

	EXPECT_FALSE(SavedEquipped(model->GetId()));
	EXPECT_TRUE(SavedEquipped(hat->GetId()));
}

TEST_F(InventorySaveTests, ItemTakenOffInBuildModeIsStillSavedEquipped) {
	const auto* const hat = GiveEquipped(HAT, eInventoryType::ITEMS);
	inventory->PushEquippedItems();
	inventory->RemoveSlot("hair");

	EXPECT_TRUE(SavedEquipped(hat->GetId()));
}
