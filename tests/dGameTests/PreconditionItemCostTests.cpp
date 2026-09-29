#include "Preconditions.h"

#include <map>

#include <gtest/gtest.h>

// The items a quickbuild's HasItem precondition takes when the build starts
// (the FV Stone Warrior pedestal: precondition 99, 5 of LOT 6194).

namespace {
	std::function<uint32_t(LOT)> Holding(const std::map<LOT, uint32_t>& inventory) {
		return [inventory](const LOT lot) {
			const auto it = inventory.find(lot);
			return it == inventory.end() ? 0u : it->second;
		};
	}
}

TEST(PreconditionItemCostTests, TakesTheCountOfTheHeldLot) {
	EXPECT_EQ(Precondition::PickItemCost({ 6194 }, 5, Holding({ { 6194, 7 } })), (ItemCost{ 6194, 5 }));
	EXPECT_EQ(Precondition::PickItemCost({ 6194 }, 5, Holding({ { 6194, 5 } })), (ItemCost{ 6194, 5 }));
}

TEST(PreconditionItemCostTests, NotEnoughTakesNothing) {
	EXPECT_FALSE(Precondition::PickItemCost({ 6194 }, 5, Holding({ { 6194, 4 } })));
	EXPECT_FALSE(Precondition::PickItemCost({ 6194 }, 5, Holding({})));
}

// A precondition that lists several LOTs is met by any of them: the first one held is taken
TEST(PreconditionItemCostTests, TakesTheFirstHeldOfSeveralLots) {
	EXPECT_EQ(Precondition::PickItemCost({ 100, 200, 300 }, 2, Holding({ { 100, 1 }, { 200, 2 }, { 300, 9 } })), (ItemCost{ 200, 2 }));
}
