#include <gtest/gtest.h>

#include <regex>
#include <set>

#include "Permissions.h"
#include "RouteUtils.h"

TEST(PermissionsTests, CatalogIsWellFormed) {
	std::set<std::string> keys;
	const std::regex name("^[a-z0-9_]{1,40}$");
	for (const auto& permission : Permissions::All()) {
		EXPECT_TRUE(keys.insert(permission.key).second) << "duplicate " << permission.key;
		// Must be a valid setting name for the Settings layer
		EXPECT_TRUE(std::regex_match(permission.key, name)) << permission.key;
		EXPECT_GE(permission.defaultLevel, permission.minLevel) << permission.key;
		// Only what players do may go below the staff floor
		if (permission.minLevel < Permissions::MIN_LEVEL) EXPECT_EQ(permission.category, "Players") << permission.key;
		EXPECT_LE(permission.defaultLevel, Permissions::MAX_LEVEL) << permission.key;
		EXPECT_FALSE(permission.title.empty());
		EXPECT_EQ(Permissions::Find(permission.key), &permission);
	}
	ASSERT_NE(Permissions::Find("permissions_manage"), nullptr);
	EXPECT_TRUE(Permissions::Find("permissions_manage")->locked);
	EXPECT_EQ(Permissions::Find("no_such_permission"), nullptr);
}

TEST(PermissionsTests, ResolveUsesValidLevelsOnly) {
	const Permissions::Permission ban{ "accounts_ban", "Accounts", "Ban", "", 4 };
	EXPECT_EQ(Permissions::Resolve(ban, ""), 4);
	EXPECT_EQ(Permissions::Resolve(ban, "3"), 3);
	EXPECT_EQ(Permissions::Resolve(ban, "9"), 9);
	EXPECT_EQ(Permissions::Resolve(ban, "1"), 1);
	// Out of range or junk: the default, never a level that would let players in or lock GM 9 out
	EXPECT_EQ(Permissions::Resolve(ban, "0"), 4);
	EXPECT_EQ(Permissions::Resolve(ban, "10"), 4);
	EXPECT_EQ(Permissions::Resolve(ban, "-1"), 4);
	EXPECT_EQ(Permissions::Resolve(ban, "three"), 4);
	const Permissions::Permission locked{ "permissions_manage", "Server", "Permissions", "", 9, true };
	EXPECT_EQ(Permissions::Resolve(locked, "1"), 9);
}

TEST(PermissionsTests, UnknownPermissionIsDeniedToEveryone) {
	EXPECT_FALSE(Permissions::Allowed(9, "no_such_permission"));
	EXPECT_TRUE(Permissions::Allowed(9, "accounts_delete"));
	EXPECT_FALSE(Permissions::Allowed(8, "accounts_delete"));
}

TEST(PermissionsTests, PlayerPermissionsCanBeRaisedOrLowered) {
	const auto* api = Permissions::Find("api_access");
	ASSERT_NE(api, nullptr);
	EXPECT_EQ(Permissions::Resolve(*api, ""), 0);
	EXPECT_EQ(Permissions::Resolve(*api, "0"), 0);
	EXPECT_EQ(Permissions::Resolve(*api, "3"), 3); // e.g. only staff may use the API
	// Staff permissions still can't be handed to players
	const auto* ban = Permissions::Find("accounts_ban");
	ASSERT_NE(ban, nullptr);
	EXPECT_EQ(Permissions::Resolve(*ban, "0"), ban->defaultLevel);
}

// The rule behind RouteUtils::CanViewCharacter(context, ...), with the default levels
TEST(PermissionsTests, CanViewCharacterFollowsPermissions) {
	EXPECT_TRUE(Permissions::CanViewCharacter(0, 5, 5));   // own character (own_characters, GM 0)
	EXPECT_FALSE(Permissions::CanViewCharacter(0, 5, 6));  // someone else's
	EXPECT_FALSE(Permissions::CanViewCharacter(0, 0, 0));  // no account id never matches
	const auto viewLevel = Permissions::Level("characters_view");
	EXPECT_TRUE(Permissions::CanViewCharacter(viewLevel, 5, 6));
	if (viewLevel > 0) EXPECT_FALSE(Permissions::CanViewCharacter(viewLevel - 1, 5, 6));
}

TEST(PermissionsTests, DangerousPermissionsStayHigh) {
	// Attaching items is as strong as /gmadditem; settings can point the server at programs and folders
	EXPECT_EQ(Permissions::Level("mail_items"), 8);
	EXPECT_LT(Permissions::Level("mail_send"), Permissions::Level("mail_items"));
	ASSERT_NE(Permissions::Find("settings"), nullptr);
	EXPECT_TRUE(Permissions::Find("settings")->locked);
	EXPECT_EQ(Permissions::Resolve(*Permissions::Find("settings"), "3"), 9);
}

// The self and equal-rank rules (RouteUtils::CanManageAccount(context, ...)) with the permission levels as set
TEST(PermissionsTests, SelfAndEqualRankFollowPermissions) {
	using RouteUtils::CanManageAccount;
	const auto* tools = Permissions::Find("self_tools");
	const auto* items = Permissions::Find("self_items");
	const auto* moderation = Permissions::Find("self_moderation");
	const auto* equal = Permissions::Find("manage_equal_rank");
	ASSERT_NE(tools, nullptr);
	ASSERT_NE(items, nullptr);
	ASSERT_NE(moderation, nullptr);
	ASSERT_NE(equal, nullptr);
	const auto may = [](const Permissions::Permission& permission, const std::string& configured, uint8_t gmLevel) {
		return gmLevel >= Permissions::Resolve(permission, configured);
	};

	// Defaults: harmless tools on yourself for all staff; items, your own record and your peers only for GM 9
	EXPECT_EQ(tools->defaultLevel, 1);
	EXPECT_EQ(items->defaultLevel, 9);
	EXPECT_EQ(moderation->defaultLevel, 9);
	EXPECT_EQ(equal->defaultLevel, 9);
	EXPECT_TRUE(CanManageAccount(3, 7, 3, 7, may(*tools, "", 3), may(*equal, "", 3)));
	EXPECT_FALSE(CanManageAccount(8, 7, 8, 7, may(*items, "", 8), may(*equal, "", 8)));
	EXPECT_FALSE(CanManageAccount(8, 7, 8, 7, may(*moderation, "", 8), may(*equal, "", 8)));
	EXPECT_FALSE(CanManageAccount(8, 7, 8, 6, may(*items, "", 8), may(*equal, "", 8)));
	// GM 9 needs none of them
	EXPECT_TRUE(CanManageAccount(9, 7, 9, 7, false, false));
	EXPECT_TRUE(CanManageAccount(9, 7, 9, 6, false, false));

	// Lowered on the Permissions page: GM 8 may give themselves items and act on other GM 8s
	EXPECT_TRUE(CanManageAccount(8, 7, 8, 7, may(*items, "8", 8), may(*equal, "8", 8)));
	EXPECT_TRUE(CanManageAccount(8, 7, 8, 6, may(*items, "8", 8), may(*equal, "8", 8)));
	EXPECT_FALSE(CanManageAccount(7, 7, 7, 7, may(*items, "8", 7), may(*equal, "8", 7)));
	// Raised: nobody below GM 9 uses even the harmless tools on themselves
	EXPECT_FALSE(CanManageAccount(8, 7, 8, 7, may(*tools, "9", 8), false));

	// Even with everything lowered to 1, never a higher GM level
	EXPECT_FALSE(CanManageAccount(8, 7, 9, 6, may(*items, "1", 8), may(*equal, "1", 8)));
	EXPECT_FALSE(CanManageAccount(2, 7, 3, 6, may(*moderation, "1", 2), may(*equal, "1", 2)));
	// ...and raising their own GM level stays impossible below GM 9
	EXPECT_FALSE(RouteUtils::CanGrantGmLevel(8, 8));
	EXPECT_FALSE(RouteUtils::CanGrantGmLevel(8, 9));
}

// The dashboard and the world servers share one implementation of the rules (dCommon/AccountRules.h)
TEST(PermissionsTests, DashboardUsesTheSharedAccountRules) {
	using namespace AccountRules;
	for (const auto action : { eAccountAction::TOOLS, eAccountAction::ITEMS, eAccountAction::MODERATION }) {
		EXPECT_NE(Permissions::Find(SelfPermission(action)), nullptr) << SelfPermission(action);
		EXPECT_STREQ(RouteUtils::SelfPermission(action), SelfPermission(action));
	}
	EXPECT_NE(Permissions::Find(EQUAL_RANK_PERMISSION), nullptr);
	EXPECT_EQ(RouteUtils::ManageDenial(4, 1, 4, 2, false, false), ManageDenial(4, 1, 4, 2, false, false));
	EXPECT_EQ(RouteUtils::OPERATOR_LEVEL, OPERATOR_LEVEL);
	// With the default levels, what the dashboard allows (CanManageAccount with its permissions) is what ManageDenialNow says
	for (uint8_t actor = 1; actor <= 9; actor++) {
		for (uint8_t target = 0; target <= 9; target++) {
			for (const auto action : { eAccountAction::TOOLS, eAccountAction::ITEMS, eAccountAction::MODERATION }) {
				const bool self = Permissions::Allowed(actor, SelfPermission(action));
				const bool equal = Permissions::Allowed(actor, EQUAL_RANK_PERMISSION);
				EXPECT_EQ(ManageDenialNow(actor, 1, target, 2, action), ManageDenial(actor, 1, target, 2, self, equal));
				EXPECT_EQ(ManageDenialNow(actor, 1, actor, 1, action), ManageDenial(actor, 1, actor, 1, self, equal));
			}
		}
	}
	// The permission levels are stored for dashboardconfig.ini, which the other servers read through ConfigSync
	EXPECT_EQ(Permissions::CONFIG_FILE, "dashboardconfig.ini");
	EXPECT_TRUE(Permissions::IsPermissionSetting(Permissions::ConfigName("accounts_kick")));
	EXPECT_FALSE(Permissions::IsPermissionSetting("command_level_kick"));
}
