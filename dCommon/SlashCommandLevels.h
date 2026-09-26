#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "GeneralUtils.h"

/**
 * Who may use each in-game slash command. Every command has a GM level in the code; server owners can change it
 * without rebuilding, on the dashboard's Permissions page or with command_level_<name>=<level> in worldconfig.ini
 * (or the COMMAND_LEVEL_<NAME> environment variable). A level set on the dashboard beats the file. World servers
 * look the level up each time a command is used, so a change applies as soon as they reload their settings.
 * A command paired with a dashboard permission (the same action) uses that permission's level instead: one setting,
 * permission_<key>. A command_level_<name> value for it is an override, kept so an older setup doesn't change silently;
 * the value "permission" (FOLLOW_PERMISSION) drops an override set in a file.
 * Shared by the world server (which enforces the levels) and the dashboard (which shows and changes them).
 */
namespace SlashCommandLevels {
	constexpr uint8_t MAX_LEVEL = 9;
	// Staff commands never go below this: players (GM 0) don't get them
	constexpr uint8_t STAFF_MIN_LEVEL = 1;
	// World servers read this file's settings, so the dashboard stores levels under it
	constexpr std::string_view CONFIG_FILE = "worldconfig.ini";
	constexpr std::string_view PREFIX = "command_level_";
	// command_level_<name>=permission: a paired command follows its permission, whatever a file sets
	constexpr std::string_view FOLLOW_PERMISSION = "permission";

	// A command's name in settings, from its first alias: "Leave-Zone" -> "leave_zone", "victory!" -> "victory"
	inline std::string SettingName(std::string_view alias) {
		std::string name;
		for (const char c : alias) {
			const auto ch = static_cast<unsigned char>(c);
			if (std::isalnum(ch)) name += static_cast<char>(std::tolower(ch));
			else if (!name.empty() && name.back() != '_') name += '_';
		}
		while (!name.empty() && name.back() == '_') name.pop_back();
		return name;
	}

	inline std::string ConfigName(std::string_view name) {
		return std::string(PREFIX) + std::string(name);
	}

	inline bool IsLevelSetting(std::string_view key) {
		return key.starts_with(PREFIX);
	}

	// The lowest level a command may be set to when the code doesn't say: staff commands stay with staff
	inline uint8_t DefaultMinLevel(uint8_t defaultLevel) {
		return defaultLevel > 0 ? STAFF_MIN_LEVEL : 0;
	}

	// A usable level value: a number from the command's floor to GM 9
	inline std::optional<uint8_t> ParseLevel(uint8_t minLevel, const std::string& configValue) {
		const auto parsed = GeneralUtils::TryParse<int32_t>(configValue);
		if (!parsed || *parsed < minLevel || *parsed > MAX_LEVEL) return std::nullopt;
		return static_cast<uint8_t>(*parsed);
	}

	// Pure: the level a config value gives a command. Fixed commands always use the default, and so do bad values
	// and values below the command's floor or above GM 9.
	inline uint8_t Resolve(uint8_t defaultLevel, uint8_t minLevel, bool fixed, const std::string& configValue) {
		if (fixed || configValue.empty()) return defaultLevel;
		return ParseLevel(minLevel, configValue).value_or(defaultLevel);
	}

	struct PairedLevel {
		uint8_t level;
		bool overridden; // an explicit command_level_<name> value is used instead of the permission's level
	};

	/**
	 * Pure: the level of a command paired with a dashboard permission. A usable command_level_<name> value overrides the
	 * permission; no value, FOLLOW_PERMISSION or a bad value follows it. The command's floor still applies (e.g.
	 * /execute never below GM 8), and fixed commands always use their default.
	 */
	inline PairedLevel ResolvePaired(uint8_t defaultLevel, uint8_t minLevel, bool fixed, uint8_t permissionLevel, const std::string& configValue) {
		if (fixed) return { defaultLevel, false };
		if (configValue != FOLLOW_PERMISSION) {
			if (const auto level = ParseLevel(minLevel, configValue)) return { *level, true };
		}
		return { std::min<uint8_t>(std::max(permissionLevel, minLevel), MAX_LEVEL), false };
	}

	// Who is named as having set a command level that the upgrade to paired commands kept (server_config.updated_by, audit log)
	constexpr std::string_view UPGRADE_ACTOR = "[upgrade]";

	/**
	 * Pure: when a command first uses its dashboard permission's level on a server that already had it, the level to keep
	 * as an override so nothing changes silently (nullopt: none needed).
	 * @param hadRow the server stored this command before (a brand-new server has no rows and just follows the permissions)
	 * @param followedBefore the stored row already followed this same permission (the upgrade happened already)
	 * @param oldDefaultLevel the level the stored row says the code gave it
	 * @param configValue its command_level_<name> value now: a usable one is already an override and stays as it is
	 * @param permissionLevel the level it gets by following the permission (ResolvePaired without a value)
	 */
	inline std::optional<uint8_t> UpgradeOverride(bool hadRow, bool followedBefore, bool fixed, uint8_t oldDefaultLevel, uint8_t minLevel, const std::string& configValue, uint8_t permissionLevel) {
		if (!hadRow || followedBefore || fixed) return std::nullopt;
		if (configValue == FOLLOW_PERMISSION || ParseLevel(minLevel, configValue)) return std::nullopt;
		// The level it had: the code's (a bad value meant the code's level too), never below the floor
		const uint8_t oldLevel = std::min<uint8_t>(std::max(oldDefaultLevel, minLevel), MAX_LEVEL);
		if (oldLevel == permissionLevel) return std::nullopt;
		return oldLevel;
	}

	/**
	 * Who a command may be used on, when it acts on another player's account or character. Staff below GM 9 follow the
	 * dashboard's rules (AccountRules.h): never a higher GM level, their own level only with manage_equal_rank, and
	 * themselves only with the self_* permission for the kind of action.
	 */
	enum class eTargetRule : uint8_t {
		NONE,       // doesn't act on other players (or only looks)
		TOOLS,      // on themselves with self_tools (kick, move)
		ITEMS,      // on themselves with self_items (mail items)
		MODERATION, // on themselves with self_moderation (ban, mute)
		OTHERS,     // checked on other players only: on themselves it works as it always did, like the self-only dev commands
	};

	// The name stored for the dashboard ("" for NONE)
	inline std::string_view TargetRuleName(eTargetRule rule) {
		switch (rule) {
		case eTargetRule::TOOLS: return "tools";
		case eTargetRule::ITEMS: return "items";
		case eTargetRule::MODERATION: return "moderation";
		case eTargetRule::OTHERS: return "others";
		default: return "";
		}
	}

	inline eTargetRule TargetRuleFromName(std::string_view name) {
		for (const auto rule : { eTargetRule::TOOLS, eTargetRule::ITEMS, eTargetRule::MODERATION, eTargetRule::OTHERS }) {
			if (TargetRuleName(rule) == name) return rule;
		}
		return eTargetRule::NONE;
	}
}
