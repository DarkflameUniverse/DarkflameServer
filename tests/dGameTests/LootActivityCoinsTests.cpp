#include "Loot.h"
#include "CDCurrencyTableTable.h"

#include <gtest/gtest.h>

// ActivityRewards coins: the reward's ChallengeRating is the CurrencyTable npcminlevel, level 1 when that level has
// no row, as live rolled them.

namespace {
	CDCurrencyTable Row(const uint32_t index, const uint32_t level, const uint32_t min, const uint32_t max) {
		return CDCurrencyTable{ .currencyIndex = index, .npcminlevel = level, .minvalue = min, .maxvalue = max, .id = 0 };
	}

	// CurrencyTable index 1 as the client data has it
	const std::vector<CDCurrencyTable> index1 = { Row(1, 1, 3, 5), Row(1, 2, 5, 9), Row(1, 3, 9, 15), Row(1, 4, 30, 50), Row(1, 5, 60, 100) };
}

// FV foot races (ChallengeRating 4): live gave 36 and 48 coins, which only level 4 (30-50) fits
TEST(LootActivityCoinsTests, ChallengeRatingPicksTheLevel) {
	EXPECT_EQ(Loot::GetActivityCoinRange(index1, 4), std::make_pair(30u, 50u));
}

// Quickbuilds, wishing wells and chests (ChallengeRating 1)
TEST(LootActivityCoinsTests, ChallengeRatingOneIsLevelOne) {
	EXPECT_EQ(Loot::GetActivityCoinRange(index1, 1), std::make_pair(3u, 5u));
}

// Survival and shooting galleries: their ChallengeRating has no row, so level 1 is used
TEST(LootActivityCoinsTests, MissingLevelFallsBackToLevelOne) {
	EXPECT_EQ(Loot::GetActivityCoinRange(index1, 7), std::make_pair(3u, 5u));
	EXPECT_EQ(Loot::GetActivityCoinRange(index1, 0), std::make_pair(3u, 5u));
}

// Frakjaw's chest (activity 58, ChallengeRating 6): index 125 has only a level 6 row of 500, which live split
// between a team of 2
TEST(LootActivityCoinsTests, IndexWithOnlyTheChallengeLevel) {
	const std::vector<CDCurrencyTable> index125 = { Row(125, 6, 500, 500) };
	EXPECT_EQ(Loot::GetActivityCoinRange(index125, 6), std::make_pair(500u, 500u));
	EXPECT_EQ(Loot::GetActivityCoinRange(index125, 2), std::make_pair(0u, 0u));
}

TEST(LootActivityCoinsTests, NoRowsNoCoins) {
	EXPECT_EQ(Loot::GetActivityCoinRange(std::vector<CDCurrencyTable>{}, 1), std::make_pair(0u, 0u));
}
