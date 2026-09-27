#include <gtest/gtest.h>

#include "Contraband.h"
#include "eGameMasterLevel.h"

using Contraband::HeldItem;
using eAction = IContraband::eContrabandAction;

TEST(ContrabandTests, FindsListedItemsOnly) {
	Contraband::List list{ { 14128, { "Atlantis Squid Helm", eAction::REMOVE } }, { 6655, { "Stig's Helmet", eAction::FLAG } } };
	const std::vector<HeldItem> items{
		{ 1, 6086, 1, eInventoryType::ITEMS },
		{ 2, 14128, 1, eInventoryType::VAULT_ITEMS },
		{ 3, 6655, 2, eInventoryType::ITEMS },
		{ 4, 6655, 0, eInventoryType::ITEMS }, // an empty stack is nothing
	};
	const auto found = Contraband::Find(items, list);
	ASSERT_EQ(found.size(), 2u);
	EXPECT_EQ(found[0].item.id, 2);
	EXPECT_EQ(found[0].entry.action, eAction::REMOVE);
	EXPECT_EQ(found[1].item.id, 3);
	EXPECT_EQ(found[1].entry.reason, "Stig's Helmet");
	EXPECT_TRUE(Contraband::Find(items, {}).empty());
}

TEST(ContrabandTests, StaffAreSkippedUnlessConfigured) {
	EXPECT_TRUE(Contraband::Applies(eGameMasterLevel::CIVILIAN, true));
	EXPECT_FALSE(Contraband::Applies(eGameMasterLevel::MODERATOR, true));
	EXPECT_TRUE(Contraband::Applies(eGameMasterLevel::MODERATOR, false));
}

TEST(ContrabandTests, OwnInventoryMovesAreNotNewItems) {
	EXPECT_TRUE(Contraband::CountsAsAdded(eLootSourceType::TRADE, eInventoryType::INVALID));
	EXPECT_TRUE(Contraband::CountsAsAdded(eLootSourceType::NONE, eInventoryType::INVALID));
	EXPECT_FALSE(Contraband::CountsAsAdded(eLootSourceType::NONE, eInventoryType::VAULT_ITEMS));
	EXPECT_FALSE(Contraband::CountsAsAdded(eLootSourceType::RELOCATE, eInventoryType::INVALID));
	EXPECT_FALSE(Contraband::CountsAsAdded(eLootSourceType::INVENTORY, eInventoryType::INVALID));
}
