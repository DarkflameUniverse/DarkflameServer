#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDLootMatrixTable.h"
#include "CDPackageComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "eReplicaComponentType.h"

#include "dGameMessagesTests/GameMessageTestUtils.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// Opening a package: the client plays the item's use sound on a successful UseItemResult and a failure sound
// otherwise; live sent true for packages that opened (183 of the 187 in the captures).
class UseItemResultTests : public GameDependenciesTest {
protected:
	static constexpr LOT PACKAGE = 2998;
	static constexpr uint32_t PACKAGE_COMPONENT = 97000;
	static constexpr uint32_t LOOT_MATRIX = 97001;

	std::unique_ptr<Entity> entity;
	InventoryComponent* inventoryComponent{};

	void SetUp() override {
		SetUpDependencies();
		const auto itemComponent = static_cast<uint32_t>(96000 + PACKAGE);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		registry.insert_or_assign(static_cast<uint64_t>(PACKAGE), itemComponent);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(PACKAGE), itemComponent);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::PACKAGE) << 32 | static_cast<uint64_t>(PACKAGE), PACKAGE_COMPONENT);
		CDItemComponent component{};
		component.id = itemComponent;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(itemComponent, component);
		CDClientManager::GetEntriesMutable<CDPackageComponentTable>().push_back({ PACKAGE_COMPONENT, LOOT_MATRIX, 0 });
		CDClientManager::GetEntriesMutable<CDLootMatrixTable>()[LOOT_MATRIX] = {}; // an empty package

		entity = std::make_unique<Entity>(1, info);
		inventoryComponent = entity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		entity.reset();
		CDClientManager::GetEntriesMutable<CDPackageComponentTable>().clear();
		CDClientManager::GetEntriesMutable<CDLootMatrixTable>().erase(LOOT_MATRIX);
		TearDownDependencies();
	}
};

TEST_F(UseItemResultTests, AnOpenedPackageReportsSuccess) {
	auto* const items = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	auto* const package = new Item(0x7000, PACKAGE, items, 0, 2, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);

	const auto results = SentGameMessages<GameMessages::UseItemResult>(Capture([&] { package->UseNonEquip(package); }));

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results[0].itemTemplateID, PACKAGE);
	EXPECT_TRUE(results[0].useItemResult);
	EXPECT_EQ(package->GetCount(), 1u);
}
