#include "PermissionGrants.h"

#include <algorithm>

#include "GeneralUtils.h"
#include "Permissions.h"

namespace {
	using PermissionGrants::eKind;
	using PermissionGrants::eMatch;

	constexpr uint8_t STAFF_FLOOR = 1; // commands with a higher floor are never granted (SlashCommandLevels::STAFF_MIN_LEVEL)

	// A deny anywhere beats every allow
	eMatch Combine(eMatch a, eMatch b) {
		if (a == eMatch::DENY || b == eMatch::DENY) return eMatch::DENY;
		if (a == eMatch::ALLOW || b == eMatch::ALLOW) return eMatch::ALLOW;
		return eMatch::NONE;
	}

	eMatch Of(const PermissionGrants::Rule& rule) {
		return rule.deny ? eMatch::DENY : eMatch::ALLOW;
	}
}

namespace PermissionGrants {
	std::optional<eKind> ParseKind(std::string_view name) {
		if (name == "permission") return eKind::PERMISSION;
		if (name == "command") return eKind::COMMAND;
		if (name == "permission_group") return eKind::PERMISSION_GROUP;
		if (name == "command_group") return eKind::COMMAND_GROUP;
		return std::nullopt;
	}

	std::string_view KindName(eKind kind) {
		switch (kind) {
		case eKind::PERMISSION: return "permission";
		case eKind::COMMAND: return "command";
		case eKind::PERMISSION_GROUP: return "permission_group";
		default: return "command_group";
		}
	}

	void Held::Add(std::string_view kind, std::string name, bool deny, int64_t expiresAt) {
		const auto parsed = ParseKind(kind);
		if (parsed) rules.push_back({ *parsed, std::move(name), deny, expiresAt });
	}

	eMatch ForPermission(const Held& held, std::string_view key, int64_t now) {
		const auto* permission = Permissions::Find(std::string(key));
		if (!permission || permission->locked) return eMatch::NONE;
		eMatch match = eMatch::NONE;
		for (const auto& rule : held.rules) {
			if (!InForce(rule, now)) continue;
			if ((rule.kind == eKind::PERMISSION && rule.name == key) || (rule.kind == eKind::PERMISSION_GROUP && rule.name == permission->category)) {
				match = Combine(match, Of(rule));
			}
		}
		return match;
	}

	eMatch ForCommand(const Held& held, const Command& command, int64_t now) {
		eMatch match = command.permission.empty() ? eMatch::NONE : ForPermission(held, command.permission, now);
		for (const auto& rule : held.rules) {
			if (!InForce(rule, now)) continue;
			if (rule.kind == eKind::COMMAND && rule.name == command.name) match = Combine(match, Of(rule));
			else if (rule.kind == eKind::COMMAND_GROUP) {
				const auto level = CommandGroupLevel(rule.name);
				if (level && command.level <= *level) match = Combine(match, Of(rule));
			}
		}
		return match;
	}

	bool MayUseCommand(uint8_t playerLevel, uint8_t accountLevel, const Command& command, const Held* held, int64_t now) {
		const bool byLevel = playerLevel >= command.level;
		if (!held || command.fixed) return byLevel;
		auto match = ForCommand(*held, command, now);
		// A floor above GM 1 is a safety limit of the code (e.g. /execute): grants don't take anyone below it
		if (match == eMatch::ALLOW && command.minLevel > STAFF_FLOOR && playerLevel < command.minLevel) match = eMatch::NONE;
		return Decide(byLevel, match, accountLevel);
	}

	std::vector<std::string> PermissionGroups() {
		std::vector<std::string> groups;
		for (const auto& permission : Permissions::All()) {
			if (std::ranges::find(groups, permission.category) == groups.end()) groups.push_back(permission.category);
		}
		return groups;
	}

	std::vector<std::string> GroupPermissions(std::string_view category) {
		std::vector<std::string> keys;
		for (const auto& permission : Permissions::All()) {
			if (permission.category == category && !permission.locked) keys.push_back(permission.key);
		}
		return keys;
	}

	std::optional<uint8_t> CommandGroupLevel(std::string_view name) {
		if (name.size() != 1 || name[0] < '1' || name[0] > '9') return std::nullopt;
		return static_cast<uint8_t>(name[0] - '0');
	}

	std::string Refusal(eKind kind, const std::string& name, uint8_t grantorLevel,
		const std::function<bool(const std::string&)>& holdsPermission,
		const std::function<std::optional<Command>(const std::string&)>& findCommand,
		const std::function<bool(const Command&)>& holdsCommand) {
		switch (kind) {
		case eKind::PERMISSION: {
			const auto* permission = Permissions::Find(name);
			if (!permission) return "Unknown permission " + name;
			if (permission->locked) return permission->key + " is always GM 9 only and can't be granted";
			if (!holdsPermission(name)) return "You can't grant " + name + ": you don't have it yourself";
			return "";
		}
		case eKind::PERMISSION_GROUP: {
			const auto keys = GroupPermissions(name);
			if (keys.empty()) return "Unknown permission group " + name;
			for (const auto& key : keys) {
				if (!holdsPermission(key)) return "You can't grant every " + name + " permission: you don't have " + key + " yourself";
			}
			return "";
		}
		case eKind::COMMAND: {
			const auto command = findCommand(name);
			if (!command) return "Unknown command " + name + " (the world servers list their commands when they start)";
			if (command->fixed) return "/" + name + " has a fixed level; grants don't apply to it";
			if (command->minLevel > STAFF_FLOOR) return "/" + name + " never goes below GM " + std::to_string(command->minLevel) + "; grants can't change that";
			if (!holdsCommand(*command)) return "You can't grant /" + name + ": you may not use it yourself";
			return "";
		}
		case eKind::COMMAND_GROUP: {
			const auto level = CommandGroupLevel(name);
			if (!level) return "A command group is a GM level from 1 to 9";
			if (grantorLevel < *level) return "You can't grant every command up to GM " + name + ": your GM level is " + std::to_string(grantorLevel);
			return "";
		}
		}
		return "Unknown kind of grant";
	}

	std::string Describe(eKind kind, const std::string& name) {
		switch (kind) {
		case eKind::PERMISSION: return "the " + name + " permission";
		case eKind::COMMAND: return "/" + name;
		case eKind::PERMISSION_GROUP: return "every " + name + " permission";
		default: return "every command up to GM " + name;
		}
	}
}
