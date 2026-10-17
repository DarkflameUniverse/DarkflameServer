#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDDeletionRestrictionsTable.h"
#include "Entity.h"
#include "eDeletionRestrictionsCheckType.h"
#include "InventoryComponent.h"

#include <gtest/gtest.h>

// DeletionRestrictions rows as the client's LWOInventoryComponent_Common::CheckDeletionRestrictionIndex (0x00c94c20)
// reads them. The test zone is map 1.
class DeletionRestrictionTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};

	void SetUp() override {
		SetUpDependencies();
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);

		using enum eDeletionRestrictionsCheckType;
		Row(1, true, ALWAYS_RESTRICTED, {});
		Row(2, false, ALWAYS_RESTRICTED, {});
		Row(3, true, ZONE, { 1, 1100 });
		Row(4, true, ZONE, { 1000 });
		Row(5, true, ANY_RESTRICTION, { 1, 3 });
		Row(6, true, ALL_RESTRICTIONS, { 1, 3 });
		Row(7, true, ALL_RESTRICTIONS, { 3, 7 });
		Row(8, true, ZONE, {});
		Row(9, true, LOTS_INCLUDED, { 6416 });
		Row(10, true, MAX, { 1 });
	}

	void TearDown() override {
		CDClientManager::GetEntriesMutable<CDDeletionRestrictionsTable>().clear();
		entity.reset();
		TearDownDependencies();
	}

	static void Row(const uint32_t id, const bool restricted, const eDeletionRestrictionsCheckType checkType, const std::vector<uint32_t>& ids) {
		CDClientManager::GetEntriesMutable<CDDeletionRestrictionsTable>().insert_or_assign(id, CDDeletionRestriction{ id, restricted, ids, checkType });
	}

	bool Allows(const uint32_t row) const {
		std::set<uint32_t> checked;
		return inventory->CheckDeletionRestriction(1234, row, checked);
	}
};

TEST_F(DeletionRestrictionTests, AlwaysRestrictedOnlyWhenRestricted) {
	EXPECT_FALSE(Allows(1));
	EXPECT_TRUE(Allows(2));
}

TEST_F(DeletionRestrictionTests, ZoneAllowsOnlyInTheListedZones) {
	EXPECT_TRUE(Allows(3));
	EXPECT_FALSE(Allows(4));
}

TEST_F(DeletionRestrictionTests, AnyAndAllCombineOtherRows) {
	EXPECT_TRUE(Allows(5));  // always restricted OR in zone 1
	EXPECT_FALSE(Allows(6)); // always restricted AND in zone 1
}

TEST_F(DeletionRestrictionTests, ARowThatNamesItselfDoesNotLoop) {
	EXPECT_TRUE(Allows(7));
}

TEST_F(DeletionRestrictionTests, MissingRowsAndBadRowsAllowIt) {
	EXPECT_TRUE(Allows(99));
	EXPECT_TRUE(Allows(8));  // no valid ids
	EXPECT_TRUE(Allows(10)); // unknown check type
}

TEST_F(DeletionRestrictionTests, LotsIncludedNeedsAnotherItemOfThoseLots) {
	// The player has no other rocket
	EXPECT_FALSE(Allows(9));
	EXPECT_FALSE(inventory->HasOtherItemsOfLots({ 6416 }, 1234, true));
	EXPECT_FALSE(inventory->HasOtherItemsOfLots({ 6416, 4713 }, 1234, false));
}
