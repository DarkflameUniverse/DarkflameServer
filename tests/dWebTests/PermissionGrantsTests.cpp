#include <gtest/gtest.h>

#include <algorithm>
#include <ctime>

#include "AccountRules.h"
#include "ApiKeyScope.h"
#include "PermissionGrants.h"
#include "Permissions.h"

using PermissionGrants::eKind;
using PermissionGrants::Held;

namespace {
	constexpr int64_t NOW = 1800000000;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	Held With(std::initializer_list<PermissionGrants::Rule> rules) {
		Held held;
		held.rules = rules;
		return held;
	}

	PermissionGrants::Rule Allow(eKind kind, std::string name, int64_t expiresAt = 0) { return { kind, std::move(name), false, expiresAt }; }
	PermissionGrants::Rule Deny(eKind kind, std::string name, int64_t expiresAt = 0) { return { kind, std::move(name), true, expiresAt }; }

	// Slash commands as the world servers describe them (levels as in the code by default)
	const PermissionGrants::Command SPAWN{ "spawn", 8, 1, false, "" };
	const PermissionGrants::Command KICK{ "kick", 2, 1, false, "accounts_kick" };
	const PermissionGrants::Command EXECUTE{ "execute", 8, 8, false, "" };
	const PermissionGrants::Command PVP{ "pvp", 0, 0, false, "" };
	const PermissionGrants::Command EMOTE{ "dance", 0, 0, true, "" };
}

TEST(PermissionGrantsTests, KindNamesRoundTrip) {
	for (const auto kind : { eKind::PERMISSION, eKind::COMMAND, eKind::PERMISSION_GROUP, eKind::COMMAND_GROUP }) {
		EXPECT_EQ(PermissionGrants::ParseKind(PermissionGrants::KindName(kind)), kind);
	}
	EXPECT_FALSE(PermissionGrants::ParseKind("role").has_value());
	Held held;
	held.Add("permission", "accounts_ban", false, 0);
	held.Add("role", "admin", false, 0); // a kind this version doesn't know counts for nothing
	ASSERT_EQ(held.rules.size(), 1u);
	EXPECT_EQ(held.rules[0].kind, eKind::PERMISSION);
}

TEST(PermissionGrantsTests, GrantAllowsWhatTheLevelDoesNot) {
	const auto held = With({ Allow(eKind::PERMISSION, "accounts_ban") });
	EXPECT_FALSE(Permissions::Allowed(2, "accounts_ban", nullptr));
	EXPECT_TRUE(Permissions::Allowed(2, "accounts_ban", nullptr, &held));
	EXPECT_TRUE(Permissions::Allowed(0, "accounts_ban", nullptr, &held)); // even a player: it's given to them by name
	EXPECT_FALSE(Permissions::Allowed(2, "accounts_delete", nullptr, &held));
	// An API key still only does what its scope names
	ApiKeys::Scope scope;
	scope.permissions = { "accounts_kick" };
	EXPECT_FALSE(Permissions::Allowed(2, "accounts_ban", &scope, &held));
}

TEST(PermissionGrantsTests, DenyTakesAwayWhatTheLevelAllowsExceptFromOperators) {
	const auto held = With({ Deny(eKind::PERMISSION, "accounts_kick") });
	EXPECT_TRUE(Permissions::Allowed(5, "accounts_kick", nullptr));
	EXPECT_FALSE(Permissions::Allowed(5, "accounts_kick", nullptr, &held));
	EXPECT_FALSE(Permissions::Allowed(8, "accounts_kick", nullptr, &held));
	// GM 9 can't be locked out
	EXPECT_TRUE(Permissions::Allowed(9, "accounts_kick", nullptr, &held));
	// A deny beats a grant of the same thing
	const auto both = With({ Allow(eKind::PERMISSION, "accounts_ban"), Deny(eKind::PERMISSION_GROUP, "Accounts") });
	EXPECT_FALSE(Permissions::Allowed(2, "accounts_ban", nullptr, &both));
	EXPECT_FALSE(Permissions::Allowed(5, "accounts_kick", nullptr, &both));
	EXPECT_TRUE(Permissions::Allowed(5, "moderate_names", nullptr, &both)); // not in the group
}

TEST(PermissionGrantsTests, ExpiredGrantsCountForNothing) {
	const auto now = Now();
	const auto expired = With({ Allow(eKind::PERMISSION, "accounts_ban", now - 1), Deny(eKind::PERMISSION, "accounts_kick", now - 1) });
	EXPECT_FALSE(Permissions::Allowed(2, "accounts_ban", nullptr, &expired));
	EXPECT_TRUE(Permissions::Allowed(2, "accounts_kick", nullptr, &expired));
	const auto running = With({ Allow(eKind::PERMISSION, "accounts_ban", now + 3600) });
	EXPECT_TRUE(Permissions::Allowed(2, "accounts_ban", nullptr, &running));
	EXPECT_TRUE(PermissionGrants::InForce({ eKind::PERMISSION, "x", false, 0 }, NOW));
	EXPECT_TRUE(PermissionGrants::InForce({ eKind::PERMISSION, "x", false, NOW + 1 }, NOW));
	EXPECT_FALSE(PermissionGrants::InForce({ eKind::PERMISSION, "x", false, NOW }, NOW));
}

TEST(PermissionGrantsTests, GroupsCoverTheirCategoryButNeverLockedPermissions) {
	const auto accounts = With({ Allow(eKind::PERMISSION_GROUP, "Accounts") });
	EXPECT_TRUE(Permissions::Allowed(1, "accounts_ban", nullptr, &accounts));
	EXPECT_TRUE(Permissions::Allowed(1, "accounts_delete", nullptr, &accounts));
	EXPECT_FALSE(Permissions::Allowed(1, "backups", nullptr, &accounts));
	// settings and permissions_manage stay GM 9 only, whatever is granted
	const auto server = With({ Allow(eKind::PERMISSION_GROUP, "Server"), Allow(eKind::PERMISSION, "permissions_manage"), Allow(eKind::PERMISSION, "settings") });
	EXPECT_TRUE(Permissions::Allowed(3, "backups", nullptr, &server));
	EXPECT_FALSE(Permissions::Allowed(8, "permissions_manage", nullptr, &server));
	EXPECT_FALSE(Permissions::Allowed(8, "settings", nullptr, &server));
	const auto keys = PermissionGrants::GroupPermissions("Server");
	EXPECT_EQ(std::ranges::count(keys, "permissions_manage"), 0);
	EXPECT_EQ(std::ranges::count(keys, "backups"), 1);
	EXPECT_TRUE(PermissionGrants::GroupPermissions("No such category").empty());
}

TEST(PermissionGrantsTests, ForLevelAndSelfRulesFollowGrants) {
	const auto held = With({ Allow(eKind::PERMISSION, "accounts_ban"), Deny(eKind::PERMISSION, "accounts_view") });
	const auto can = Permissions::ForLevel(3, nullptr, &held);
	EXPECT_TRUE(can["accounts_ban"].get<bool>());
	EXPECT_FALSE(can["accounts_view"].get<bool>());
	EXPECT_TRUE(can["accounts_kick"].get<bool>());
	// self_items (GM 9 by default) granted to a GM 8 lets them use item tools on their own characters
	using AccountRules::eAccountAction;
	using AccountRules::eManageDenial;
	const auto selfItems = With({ Allow(eKind::PERMISSION, "self_items") });
	EXPECT_EQ(AccountRules::ManageDenialNow(8, 7, 8, 7, eAccountAction::ITEMS), eManageDenial::SELF);
	EXPECT_EQ(AccountRules::ManageDenialNow(8, 7, 8, 7, eAccountAction::ITEMS, nullptr, &selfItems), eManageDenial::NONE);
	// A grant never lets anyone act on a higher GM level
	EXPECT_EQ(AccountRules::ManageDenialNow(8, 7, 9, 6, eAccountAction::ITEMS, nullptr, &selfItems), eManageDenial::HIGHER_RANK);
}

TEST(PermissionGrantsTests, CommandsFollowTheirGrantsGroupsAndPairedPermission) {
	using PermissionGrants::MayUseCommand;
	EXPECT_FALSE(MayUseCommand(3, 3, SPAWN, nullptr, NOW));
	EXPECT_TRUE(MayUseCommand(8, 8, SPAWN, nullptr, NOW));

	const auto spawn = With({ Allow(eKind::COMMAND, "spawn") });
	EXPECT_TRUE(MayUseCommand(3, 3, SPAWN, &spawn, NOW));
	EXPECT_TRUE(MayUseCommand(0, 0, SPAWN, &spawn, NOW));
	EXPECT_FALSE(MayUseCommand(1, 1, KICK, &spawn, NOW)); // a grant of one command gives only that one
	EXPECT_TRUE(MayUseCommand(3, 3, KICK, nullptr, NOW));

	// Every command up to GM 8
	const auto group = With({ Allow(eKind::COMMAND_GROUP, "8") });
	EXPECT_TRUE(MayUseCommand(1, 1, SPAWN, &group, NOW));
	const auto lowGroup = With({ Allow(eKind::COMMAND_GROUP, "5") });
	EXPECT_FALSE(MayUseCommand(1, 1, SPAWN, &lowGroup, NOW));
	EXPECT_TRUE(MayUseCommand(1, 1, KICK, &lowGroup, NOW));

	// A paired command follows its permission's grants and denies too
	const auto kickPermission = With({ Allow(eKind::PERMISSION, "accounts_kick") });
	EXPECT_TRUE(MayUseCommand(0, 0, KICK, &kickPermission, NOW));
	const auto noKick = With({ Deny(eKind::PERMISSION_GROUP, "Accounts") });
	EXPECT_FALSE(MayUseCommand(5, 5, KICK, &noKick, NOW));
	EXPECT_TRUE(MayUseCommand(5, 9, KICK, &noKick, NOW)); // a GM 9 account playing at GM 5
}

TEST(PermissionGrantsTests, CommandDeniesExpiryFloorsAndFixedCommands) {
	using PermissionGrants::MayUseCommand;
	// A deny keeps a player from a player command, and a staff member from one their level allows
	const auto noPvp = With({ Deny(eKind::COMMAND, "pvp"), Deny(eKind::COMMAND, "spawn") });
	EXPECT_FALSE(MayUseCommand(0, 0, PVP, &noPvp, NOW));
	EXPECT_FALSE(MayUseCommand(8, 8, SPAWN, &noPvp, NOW));
	EXPECT_TRUE(MayUseCommand(9, 9, SPAWN, &noPvp, NOW));
	// A deny beats a grant
	const auto both = With({ Allow(eKind::COMMAND_GROUP, "9"), Deny(eKind::COMMAND, "spawn") });
	EXPECT_FALSE(MayUseCommand(1, 1, SPAWN, &both, NOW));
	// Past its expiry a grant does nothing
	const auto expired = With({ Allow(eKind::COMMAND, "spawn", NOW - 10) });
	EXPECT_FALSE(MayUseCommand(3, 3, SPAWN, &expired, NOW));
	const auto running = With({ Allow(eKind::COMMAND, "spawn", NOW + 10) });
	EXPECT_TRUE(MayUseCommand(3, 3, SPAWN, &running, NOW));
	// /execute never goes below GM 8, grants or not
	const auto execute = With({ Allow(eKind::COMMAND, "execute"), Allow(eKind::COMMAND_GROUP, "9") });
	EXPECT_FALSE(MayUseCommand(7, 7, EXECUTE, &execute, NOW));
	EXPECT_TRUE(MayUseCommand(8, 8, EXECUTE, &execute, NOW));
	// Commands the client handles keep their fixed level
	const auto emote = With({ Deny(eKind::COMMAND, "dance") });
	EXPECT_TRUE(MayUseCommand(0, 0, EMOTE, &emote, NOW));
}

TEST(PermissionGrantsTests, NobodyGrantsWhatTheyDoNotHold) {
	const auto commands = std::vector<PermissionGrants::Command>{ SPAWN, KICK, EXECUTE, EMOTE };
	const auto find = [&commands](const std::string& name) -> std::optional<PermissionGrants::Command> {
		for (const auto& command : commands) if (command.name == name) return command;
		return std::nullopt;
	};
	// A GM 5 moderator with grants_manage: holds what GM 5 allows
	const uint8_t level = 5;
	const auto holds = [](const std::string& key) { return Permissions::Allowed(5, key, nullptr); };
	const auto uses = [](const PermissionGrants::Command& command) { return PermissionGrants::MayUseCommand(5, 5, command, nullptr, NOW); };
	const auto refusal = [&](eKind kind, const std::string& name) { return PermissionGrants::Refusal(kind, name, level, holds, find, uses); };

	EXPECT_EQ(refusal(eKind::PERMISSION, "accounts_kick"), "");
	EXPECT_NE(refusal(eKind::PERMISSION, "accounts_delete"), "");  // GM 9
	EXPECT_NE(refusal(eKind::PERMISSION, "permissions_manage"), ""); // locked
	EXPECT_NE(refusal(eKind::PERMISSION, "no_such_permission"), "");
	EXPECT_EQ(refusal(eKind::PERMISSION_GROUP, "Players"), "");
	EXPECT_NE(refusal(eKind::PERMISSION_GROUP, "Accounts"), ""); // has accounts_delete
	EXPECT_NE(refusal(eKind::PERMISSION_GROUP, "Nothing"), "");
	EXPECT_EQ(refusal(eKind::COMMAND, "kick"), "");
	EXPECT_NE(refusal(eKind::COMMAND, "spawn"), "");   // GM 8
	EXPECT_NE(refusal(eKind::COMMAND, "execute"), ""); // floor above GM 1
	EXPECT_NE(refusal(eKind::COMMAND, "dance"), "");   // fixed
	EXPECT_NE(refusal(eKind::COMMAND, "nothing"), "");
	EXPECT_EQ(refusal(eKind::COMMAND_GROUP, "5"), "");
	EXPECT_NE(refusal(eKind::COMMAND_GROUP, "6"), "");
	EXPECT_NE(refusal(eKind::COMMAND_GROUP, "0"), "");
	EXPECT_NE(refusal(eKind::COMMAND_GROUP, "10"), "");

	// What a grant gave the grantor counts as held (and a deny takes it away)
	const auto spawn = With({ Allow(eKind::COMMAND, "spawn"), Deny(eKind::PERMISSION, "accounts_kick") });
	const auto holdsWithGrants = [&spawn](const std::string& key) { return Permissions::Allowed(5, key, nullptr, &spawn); };
	const auto usesWithGrants = [&spawn](const PermissionGrants::Command& command) { return PermissionGrants::MayUseCommand(5, 5, command, &spawn, NOW); };
	EXPECT_EQ(PermissionGrants::Refusal(eKind::COMMAND, "spawn", level, holdsWithGrants, find, usesWithGrants), "");
	EXPECT_NE(PermissionGrants::Refusal(eKind::PERMISSION, "accounts_kick", level, holdsWithGrants, find, usesWithGrants), "");
	EXPECT_NE(PermissionGrants::Refusal(eKind::COMMAND, "kick", level, holdsWithGrants, find, usesWithGrants), ""); // paired: denied too
}
