#include <gtest/gtest.h>

#include "SlashCommandLevels.h"

TEST(SlashCommandLevelsTests, SettingNameFromAlias) {
	EXPECT_EQ(SlashCommandLevels::SettingName("kick"), "kick");
	EXPECT_EQ(SlashCommandLevels::SettingName("removeIgnore"), "removeignore");
	EXPECT_EQ(SlashCommandLevels::SettingName("leave-zone"), "leave_zone");
	EXPECT_EQ(SlashCommandLevels::SettingName("victory!"), "victory");
	EXPECT_EQ(SlashCommandLevels::SettingName("--a--b--"), "a_b");
	EXPECT_EQ(SlashCommandLevels::ConfigName("kick"), "command_level_kick");
	EXPECT_TRUE(SlashCommandLevels::IsLevelSetting("command_level_kick"));
	EXPECT_FALSE(SlashCommandLevels::IsLevelSetting("permission_accounts_kick"));
}

TEST(SlashCommandLevelsTests, StaffCommandsStayWithStaff) {
	EXPECT_EQ(SlashCommandLevels::DefaultMinLevel(0), 0);
	EXPECT_EQ(SlashCommandLevels::DefaultMinLevel(2), SlashCommandLevels::STAFF_MIN_LEVEL);
	EXPECT_EQ(SlashCommandLevels::DefaultMinLevel(9), SlashCommandLevels::STAFF_MIN_LEVEL);
}

TEST(SlashCommandLevelsTests, Resolve) {
	// No value: the code's level
	EXPECT_EQ(SlashCommandLevels::Resolve(2, 1, false, ""), 2);
	// Raised and lowered within range
	EXPECT_EQ(SlashCommandLevels::Resolve(2, 1, false, "5"), 5);
	EXPECT_EQ(SlashCommandLevels::Resolve(8, 1, false, "1"), 1);
	EXPECT_EQ(SlashCommandLevels::Resolve(0, 0, false, "9"), 9);
	// Below the floor, above GM 9 or not a number: the default
	EXPECT_EQ(SlashCommandLevels::Resolve(8, 1, false, "0"), 8);
	EXPECT_EQ(SlashCommandLevels::Resolve(8, 8, false, "6"), 8);
	EXPECT_EQ(SlashCommandLevels::Resolve(2, 1, false, "10"), 2);
	EXPECT_EQ(SlashCommandLevels::Resolve(2, 1, false, "-1"), 2);
	EXPECT_EQ(SlashCommandLevels::Resolve(2, 1, false, "abc"), 2);
	// Fixed commands ignore the setting
	EXPECT_EQ(SlashCommandLevels::Resolve(0, 0, true, "5"), 0);
}

// A command paired with a dashboard permission uses the permission's level; its own value is an override
TEST(SlashCommandLevelsTests, PairedCommandsFollowTheirPermission) {
	using SlashCommandLevels::ResolvePaired;
	// No value: the permission's level, whatever the code's default was (/mute: code GM 6, accounts_mute GM 2)
	EXPECT_EQ(ResolvePaired(6, 1, false, 2, "").level, 2);
	EXPECT_FALSE(ResolvePaired(6, 1, false, 2, "").overridden);
	// The permission changes, the command changes with it
	EXPECT_EQ(ResolvePaired(6, 1, false, 5, "").level, 5);
	// An explicit value (an older setup's command_level_mute) overrides it
	EXPECT_EQ(ResolvePaired(6, 1, false, 2, "6").level, 6);
	EXPECT_TRUE(ResolvePaired(6, 1, false, 2, "6").overridden);
	EXPECT_TRUE(ResolvePaired(6, 1, false, 2, "2").overridden); // even when it is the same level: it stays put when the permission moves
	// "permission" (set on the page over a file's value) and bad values follow the permission
	EXPECT_EQ(ResolvePaired(6, 1, false, 3, std::string(SlashCommandLevels::FOLLOW_PERMISSION)).level, 3);
	EXPECT_FALSE(ResolvePaired(6, 1, false, 3, std::string(SlashCommandLevels::FOLLOW_PERMISSION)).overridden);
	EXPECT_EQ(ResolvePaired(6, 1, false, 3, "abc").level, 3);
	EXPECT_FALSE(ResolvePaired(6, 1, false, 3, "10").overridden);
	// Floors still apply, to the permission's level and to overrides
	EXPECT_EQ(ResolvePaired(8, 8, false, 3, "").level, 8);
	EXPECT_EQ(ResolvePaired(8, 8, false, 3, "5").level, 8);
	EXPECT_FALSE(ResolvePaired(8, 8, false, 3, "5").overridden);
	EXPECT_EQ(ResolvePaired(2, 1, false, 0, "").level, 1); // a staff command never reaches players
	// Nobody above GM 9 (an unknown permission's level)
	EXPECT_EQ(ResolvePaired(2, 1, false, 10, "").level, 9);
	// Fixed commands ignore both
	EXPECT_EQ(ResolvePaired(0, 0, true, 5, "3").level, 0);
	EXPECT_FALSE(ResolvePaired(0, 0, true, 5, "3").overridden);
}

TEST(SlashCommandLevelsTests, TargetRuleNames) {
	using SlashCommandLevels::eTargetRule;
	for (const auto rule : { eTargetRule::NONE, eTargetRule::TOOLS, eTargetRule::ITEMS, eTargetRule::MODERATION, eTargetRule::OTHERS }) {
		EXPECT_EQ(SlashCommandLevels::TargetRuleFromName(SlashCommandLevels::TargetRuleName(rule)), rule);
	}
	EXPECT_EQ(SlashCommandLevels::TargetRuleName(eTargetRule::NONE), "");
	EXPECT_EQ(SlashCommandLevels::TargetRuleFromName("junk"), eTargetRule::NONE);
}
