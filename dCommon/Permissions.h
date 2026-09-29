#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

namespace ApiKeys { struct Scope; }
namespace PermissionGrants { struct Held; }

/**
 * What each GM level may do on the dashboard, and in the game for the slash commands paired with a permission. Every
 * permission has a default minimum GM level; server owners can change it without rebuilding, on the Permissions page
 * or with permission_<name>=<level> in dashboardconfig.ini (or the dashboard's PERMISSION_<NAME> environment
 * variable). A level set on the page beats the file. Levels are 1-9, and GM 9 can always do everything, so an operator
 * can never lock themselves out.
 * The dashboard owns these settings. The other servers (the worlds, for their slash commands) read the values the
 * dashboard stored in the database (ConfigSync), so every server sees the same levels and picks up changes when its
 * settings reload.
 */
namespace Permissions {
	constexpr uint8_t MIN_LEVEL = 1;    // the lowest level a staff permission can be given to: players (GM 0) never get them
	constexpr uint8_t PLAYER_LEVEL = 0; // what players can do (their own characters, leaderboards, the API) can be set this low
	constexpr uint8_t MAX_LEVEL = 9;
	// The server that owns the permission_* settings, and the file its values are stored under
	constexpr std::string_view CONFIG_FILE = "dashboardconfig.ini";
	constexpr std::string_view PREFIX = "permission_";

	struct Permission {
		std::string key;         // lower_snake_case; the config setting is permission_<key>
		std::string category;
		std::string title;
		std::string description;
		uint8_t defaultLevel;
		bool locked{};           // always the default (managing permissions themselves)
		uint8_t minLevel{ MIN_LEVEL }; // the lowest level it can be set to: PLAYER_LEVEL for what players do
	};

	const std::vector<Permission>& All();
	const Permission* Find(const std::string& key);

	std::string ConfigName(const std::string& key);

	inline bool IsPermissionSetting(std::string_view name) {
		return name.starts_with(PREFIX);
	}

	// The minimum GM level for a permission right now; unknown permissions need more than GM 9 (nobody)
	uint8_t Level(const std::string& key);

	bool Allowed(uint8_t gmLevel, const std::string& key);

	// For a request made with an API key: the owner's level must allow it AND the key's scope must name it.
	// scope nullptr (a browser session) is the plain check. grants: the account's permission grants (PermissionGrants.h),
	// which can allow what the level doesn't or deny what it does; nullptr: the level alone.
	bool Allowed(uint8_t gmLevel, const std::string& key, const ApiKeys::Scope* scope, const PermissionGrants::Held* grants = nullptr);

	// The permissions in a requested API key scope that a GM level may not give it (unknown ones, or ones it doesn't
	// have): a key can never be made with more than its maker has. Empty: all of them may be given.
	std::set<std::string> NotGrantable(uint8_t gmLevel, const std::set<std::string>& requested, const PermissionGrants::Held* grants = nullptr);

	// characters_view for anyone's character, or own_characters for the viewer's own (account 0 owns nothing)
	bool CanViewCharacter(uint8_t gmLevel, uint32_t viewerAccountId, uint32_t ownerAccountId, const ApiKeys::Scope* scope = nullptr, const PermissionGrants::Held* grants = nullptr);

	// {key: bool} for every permission, for templates and scripts
	nlohmann::json ForLevel(uint8_t gmLevel, const ApiKeys::Scope* scope = nullptr, const PermissionGrants::Held* grants = nullptr);

	// Pure: the level a config value gives a permission (bad or out-of-range values fall back to the default)
	uint8_t Resolve(const Permission& permission, const std::string& configValue);
}
