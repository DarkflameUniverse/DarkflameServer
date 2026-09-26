#include <gtest/gtest.h>

#include "GameLabels.h"
#include "IEconomyLedger.h"
#include "StatisticID.h"

TEST(GameLabelsTests, NamesComeFromTheEnums) {
	EXPECT_EQ(GameLabels::Words("FORUM_MODERATOR"), "Forum Moderator");
	EXPECT_EQ(GameLabels::Name(VAULT_MODELS), "Vault Models");
	EXPECT_EQ(GameLabels::Name(PropertyPrivacyOption::Friends), "Friends");

	const auto& labels = GameLabels::Json();
	ASSERT_EQ(labels["gmLevels"].size(), 10u);
	EXPECT_EQ(labels["gmLevels"][9]["name"], "Operator");
	for (const auto& inventory : labels["inventories"]) EXPECT_NE(inventory["value"], static_cast<int>(ALL));
	EXPECT_EQ(labels["privacy"][1]["name"], "Friends");
}

TEST(GameLabelsTests, CamelCaseNamesAreSplitIntoWords) {
	EXPECT_EQ(GameLabels::Words("SmashablesSmashed"), "Smashables Smashed");
	EXPECT_EQ(GameLabels::Name(TimeAirborneInCar), "Time Airborne In Car");
	EXPECT_EQ(GameLabels::Name(IEconomyLedger::eMapEvent::PLAYER_COIN_DROPS), "Player Coin Drops");
}
