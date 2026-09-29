#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * Permissions and slash commands granted to, or denied from, one account or character on top of what its GM level
 * allows (the permission_grants table). What someone may do:
 *
 *   allowed = (their GM level allows it OR a grant allows it) AND no deny matches it
 *
 * Denies never apply to GM 9 accounts, so an operator can't be locked out; the locked permissions (settings,
 * permissions_manage) stay GM 9 only and no grant or group covers them. A grant for a command whose floor is above GM 1
 * (e.g. /execute) never takes anyone below that floor. Grants that are removed or past their expiry count for nothing.
 * The dashboard uses the account's grants; the world servers use the account's and the logged-in character's.
 * Everything here is pure apart from reading the permission catalog and levels (Permissions.h).
 */
namespace PermissionGrants {
	constexpr std::string_view ACCOUNT = "account";
	constexpr std::string_view CHARACTER = "character";

	enum class eKind : uint8_t {
		PERMISSION,       // name: a dashboard permission key; in game it also covers the commands paired with it
		COMMAND,          // name: a slash command's settings name (SlashCommandLevels::SettingName)
		PERMISSION_GROUP, // name: a permission category: every permission in it that isn't locked
		COMMAND_GROUP,    // name: a GM level "1".."9": every command needing that level or lower
	};

	std::optional<eKind> ParseKind(std::string_view name);
	std::string_view KindName(eKind kind);

	struct Rule {
		eKind kind{};
		std::string name;
		bool deny{};
		int64_t expiresAt{}; // 0: never
	};

	// The rules of one account (and in game its logged-in character), as loaded
	struct Held {
		std::vector<Rule> rules;
		// A row from the table; a kind this version doesn't know is skipped
		void Add(std::string_view kind, std::string name, bool deny, int64_t expiresAt);
	};

	inline bool InForce(const Rule& rule, int64_t now) {
		return rule.expiresAt == 0 || rule.expiresAt > now;
	}

	enum class eMatch : uint8_t { NONE, ALLOW, DENY };

	// What the rules say about a dashboard permission (its own rules and its category's). A deny beats an allow.
	// Locked or unknown permissions never match.
	eMatch ForPermission(const Held& held, std::string_view key, int64_t now);

	// A slash command as the grant rules see it
	struct Command {
		std::string name;       // settings name
		uint8_t level{};        // the level it needs now
		uint8_t minLevel{};     // its floor
		bool fixed{};           // its level can't change (the client acts on it by itself); grants don't apply
		std::string permission; // the dashboard permission it is paired with, if any
	};

	// What the rules say about a command: its own rules, command groups at or above its level, and for a paired
	// command its permission's rules. A deny beats an allow.
	eMatch ForCommand(const Held& held, const Command& command, int64_t now);

	// Whether denies may apply to an account at this GM level (never to GM 9)
	inline bool Deniable(uint8_t accountLevel) {
		return accountLevel < 9;
	}

	inline bool Decide(bool byLevel, eMatch match, uint8_t accountLevel) {
		if (match == eMatch::DENY && Deniable(accountLevel)) return false;
		return byLevel || match == eMatch::ALLOW;
	}

	/**
	 * Whether someone may use a command. playerLevel: the character's current GM level (what the level check uses);
	 * accountLevel: the account's (decides whether denies apply). held nullptr: the level alone.
	 */
	bool MayUseCommand(uint8_t playerLevel, uint8_t accountLevel, const Command& command, const Held* held, int64_t now);

	// The permission groups: the categories of the dashboard permissions, in catalog order
	std::vector<std::string> PermissionGroups();
	// The keys a permission group covers (never the locked permissions); empty: no such group
	std::vector<std::string> GroupPermissions(std::string_view category);
	// A command group's GM level (1-9); nullopt: not a command group name
	std::optional<uint8_t> CommandGroupLevel(std::string_view name);

	/**
	 * Why someone may not grant or deny this (empty: they may). Nobody hands out more than they hold themselves:
	 * holdsPermission(key) is whether the grantor has a dashboard permission now; findCommand(name) looks a command up;
	 * holdsCommand(command) is whether the grantor may use it; grantorLevel is the grantor's GM level (command groups
	 * need at least their level). Locked permissions, fixed commands and commands with a floor above GM 1 can't be
	 * granted at all.
	 */
	std::string Refusal(eKind kind, const std::string& name, uint8_t grantorLevel,
		const std::function<bool(const std::string&)>& holdsPermission,
		const std::function<std::optional<Command>(const std::string&)>& findCommand,
		const std::function<bool(const Command&)>& holdsCommand);

	// "the accounts_kick permission", "/spawn", "every Accounts permission", "every command up to GM 3"
	std::string Describe(eKind kind, const std::string& name);
}
