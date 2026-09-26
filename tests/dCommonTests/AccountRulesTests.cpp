#include <gtest/gtest.h>

#include "AccountRules.h"
#include "Permissions.h"
#include "Game.h"
#include "dConfig.h"

using namespace AccountRules;

TEST(AccountRulesTests, RankAndSelfRules) {
	// GM 9 acts on anyone, themselves and other GM 9s included
	EXPECT_EQ(ManageDenial(9, 1, 9, 2, false, false), eManageDenial::NONE);
	EXPECT_EQ(ManageDenial(9, 1, 9, 1, false, false), eManageDenial::NONE);
	// Below GM 9: never a higher level, whatever the permissions say
	EXPECT_EQ(ManageDenial(8, 1, 9, 2, true, true), eManageDenial::HIGHER_RANK);
	// Their own level only with manage_equal_rank
	EXPECT_EQ(ManageDenial(4, 1, 4, 2, true, false), eManageDenial::EQUAL_RANK);
	EXPECT_EQ(ManageDenial(4, 1, 4, 2, false, true), eManageDenial::NONE);
	// Themselves only with the self permission
	EXPECT_EQ(ManageDenial(4, 1, 4, 1, false, true), eManageDenial::SELF);
	EXPECT_EQ(ManageDenial(4, 1, 4, 1, true, false), eManageDenial::NONE);
	// Lower levels: fine
	EXPECT_EQ(ManageDenial(2, 1, 0, 2, false, false), eManageDenial::NONE);
}

TEST(AccountRulesTests, MessagesNameThePermission) {
	EXPECT_NE(DenialMessage(eManageDenial::SELF, eAccountAction::ITEMS).find("self_items"), std::string::npos);
	EXPECT_NE(DenialMessage(eManageDenial::SELF, eAccountAction::MODERATION).find("self_moderation"), std::string::npos);
	EXPECT_NE(DenialMessage(eManageDenial::SELF, eAccountAction::TOOLS).find("self_tools"), std::string::npos);
	EXPECT_NE(DenialMessage(eManageDenial::EQUAL_RANK, eAccountAction::TOOLS).find("manage_equal_rank"), std::string::npos);
	EXPECT_NE(DenialMessage(eManageDenial::HIGHER_RANK, eAccountAction::TOOLS).find("higher GM level"), std::string::npos);
	EXPECT_EQ(DenialMessage(eManageDenial::NONE, eAccountAction::TOOLS), "");
	EXPECT_NE(LastOperatorMessage("banned").find("banned"), std::string::npos);
}

// ManageDenialNow reads the self_* and manage_equal_rank levels from the config, as any server sees them
TEST(AccountRulesTests, ManageDenialNowUsesPermissionLevels) {
	auto* previous = Game::config;
	dConfig config("does_not_exist_config.ini");
	Game::config = &config;

	// Defaults: self_tools GM 1, self_items / self_moderation / manage_equal_rank GM 9
	EXPECT_EQ(ManageDenialNow(3, 7, 3, 7, eAccountAction::TOOLS), eManageDenial::NONE);
	EXPECT_EQ(ManageDenialNow(8, 7, 8, 7, eAccountAction::ITEMS), eManageDenial::SELF);
	EXPECT_EQ(ManageDenialNow(8, 7, 8, 7, eAccountAction::MODERATION), eManageDenial::SELF);
	EXPECT_EQ(ManageDenialNow(8, 7, 8, 6, eAccountAction::TOOLS), eManageDenial::EQUAL_RANK);

	// Lowered on the Permissions page (values the dashboard stored)
	config.SetDatabaseValues({ { Permissions::ConfigName("self_items"), "8" }, { Permissions::ConfigName("manage_equal_rank"), "8" } }, {});
	EXPECT_EQ(ManageDenialNow(8, 7, 8, 7, eAccountAction::ITEMS), eManageDenial::NONE);
	EXPECT_EQ(ManageDenialNow(8, 7, 8, 6, eAccountAction::TOOLS), eManageDenial::NONE);
	EXPECT_EQ(ManageDenialNow(8, 7, 9, 6, eAccountAction::TOOLS), eManageDenial::HIGHER_RANK);

	Game::config = previous;
}
