#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDDeletionRestrictionsTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"

#include <gtest/gtest.h>

// The deletion checks follow the client's LWOInventoryComponent_Common::CheckDeletionRestrictionIndex (0x00c94c20).
class DeletionRestrictionTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};
	LWOOBJID nextId = 0x6000;

	void SetUp() override {
		SetUpDependencies();
		// Rows as the CDClient has them
		auto& rows = CDClientManager::GetEntriesMutable<CDDeletionRestrictionsTable>();
		rows.clear();
		rows[0] = { false, "", 0 };
		rows[500] = { true, "2001, 2002", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::LOTS_INCLUDED) };
		rows[501] = { true, "2001,2002", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::LOTS_EXCLUDED) };
		rows[502] = { true, "", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::ALWAYS_RESTRICTED) };
		rows[503] = { true, "500, 502", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::ANY_RESTRICTION) };
		rows[504] = { true, "500, 502", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::ALL_RESTRICTIONS) };
		rows[505] = { true, "505", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::ALL_RESTRICTIONS) };
		rows[506] = { true, "", GeneralUtils::ToUnderlying(eDeletionRestrictionCheckType::LOTS_INCLUDED) };

		RegisterLot(1000, 0);
		RegisterLot(1500, 500);
		RegisterLot(1501, 501);
		RegisterLot(1502, 502);
		RegisterLot(1503, 503);
		RegisterLot(1504, 504);
		RegisterLot(1505, 505);
		RegisterLot(1506, 506);
		RegisterLot(1599, 599); // no such row
		RegisterLot(2001, -1);
		RegisterLot(2002, -1);

		// The entity's own LOT has no components, so InventoryComponent doesn't look it up in a database
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot, const int32_t delResIndex) {
		const auto componentID = static_cast<uint32_t>(95000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.delResIndex = static_cast<uint32_t>(delResIndex);
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	// The inventory owns the item
	Item* Give(const LOT lot) {
		auto* const items = inventory->GetInventory(eInventoryType::ITEMS);
		return new Item(nextId++, lot, items, static_cast<uint32_t>(items->GetItems().size()), 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}
};

TEST_F(DeletionRestrictionTests, UnrestrictedRowsAllowDeleting) {
	EXPECT_TRUE(inventory->CanDelete(*Give(1000)));
	EXPECT_TRUE(inventory->CanDelete(*Give(1599)));
	EXPECT_TRUE(inventory->CanDelete(*Give(2001)));
	EXPECT_TRUE(inventory->CanDelete(*Give(1506))); // restricted row without IDs
	EXPECT_TRUE(inventory->CanDelete(*Give(1505))); // refers to itself
}

TEST_F(DeletionRestrictionTests, AlwaysRestricted) {
	EXPECT_FALSE(inventory->CanDelete(*Give(1502)));
}

TEST_F(DeletionRestrictionTests, OperatorsDeleteAnything) {
	entity->SetGMLevel(eGameMasterLevel::OPERATOR);
	EXPECT_TRUE(inventory->CanDelete(*Give(1502)));
}

TEST_F(DeletionRestrictionTests, AnyOfTheLotsMustRemain) {
	auto* const item = Give(1500);
	EXPECT_FALSE(inventory->CanDelete(*item));
	Give(2002);
	EXPECT_TRUE(inventory->CanDelete(*item));
}

TEST_F(DeletionRestrictionTests, AllOfTheLotsMustRemain) {
	auto* const item = Give(1501);
	Give(2001);
	EXPECT_FALSE(inventory->CanDelete(*item));
	Give(2002);
	EXPECT_TRUE(inventory->CanDelete(*item));
}

TEST_F(DeletionRestrictionTests, TheDeletedItemDoesNotCount) {
	auto* const item = Give(2001);
	RegisterLot(2001, 500);
	EXPECT_FALSE(inventory->CanDelete(*item));
	Give(2001);
	EXPECT_TRUE(inventory->CanDelete(*item));
}

TEST_F(DeletionRestrictionTests, AnyAndAllOfOtherRows) {
	auto* const any = Give(1503);
	auto* const all = Give(1504);
	EXPECT_FALSE(inventory->CanDelete(*any));
	EXPECT_FALSE(inventory->CanDelete(*all));
	Give(2001); // row 500 now allows it, row 502 never does
	EXPECT_TRUE(inventory->CanDelete(*any));
	EXPECT_FALSE(inventory->CanDelete(*all));
}
