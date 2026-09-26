/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SLASHCOMMANDHANDLER_H
#define SLASHCOMMANDHANDLER_H

#include "RakNetTypes.h"
#include "eGameMasterLevel.h"
#include "dCommonVars.h"
#include "AccountRules.h"
#include "SlashCommandLevels.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Entity;

struct Command {
	std::string help;
	std::string info;
	std::vector<std::string> aliases;
	std::function<void(Entity*, const SystemAddress&,const std::string)> handle;
	// The default level; command_level_<name> in the settings (e.g. from the dashboard) can change it
	eGameMasterLevel requiredLevel = eGameMasterLevel::OPERATOR;
	// The lowest level the settings may give it; unset: GM 1 for staff commands (never players), GM 0 for the rest
	std::optional<eGameMasterLevel> minLevel;
	// Always requiredLevel, whatever the settings say
	bool fixedLevel = false;
	// Set by RegisterCommand for commands the game client acts on by itself (their level is fixed)
	bool clientHandled = false;
	// Why the command has a floor or a fixed level, shown on the dashboard
	std::string levelNote;
	// The dashboard permission (Permissions.cpp key) that does the same thing. The command then needs that permission's
	// level (one setting for both); a command_level_<name> value still overrides it, so older setups don't change silently
	std::string dashboardPermission;
	// Who it may be used on when it acts on another player: the dashboard's self and rank rules (the handler checks them
	// with SlashCommandHandler::MayActOn; this tells the dashboard which rule it follows)
	SlashCommandLevels::eTargetRule targetRule = SlashCommandLevels::eTargetRule::NONE;
	// Set by RegisterCommand: the command's name in the settings, from its first alias
	std::string name;
};

// A player a command acts on, in this world or not
struct CommandTarget {
	uint32_t accountId{};
	uint8_t gmLevel{};      // the account's GM level (the character's in-game level if that is higher)
	LWOOBJID characterId{};
	std::string name;       // the character's name
	Entity* entity{};       // set when the player is in this world
};

namespace SlashCommandHandler {
	void HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr);
	void SendAnnouncement(const std::string& title, const std::string& message);
	void RegisterCommand(Command info);
	void Startup();

	// The command an alias runs (nullptr: none)
	const Command* FindCommand(const std::string& alias);

	// The level needed to use a command right now: its default unless the settings change it
	eGameMasterLevel GetRequiredLevel(const Command& command);

	// Store the registered commands in the database for the dashboard (only what changed)
	void ReportCommands();

	// A player in this world, as a command target
	std::optional<CommandTarget> TargetOf(Entity* player);

	// The character with this name: in this world if they are here, else from the database. nullopt: no such character
	std::optional<CommandTarget> FindTarget(const std::string& name);

	/**
	 * Pure (apart from the permission levels): whether staff at actorLevel may use a command with this rule on the target
	 * account. The same rules as the dashboard's tools: GM 9 may act on anyone; below GM 9 never on a higher GM level, on
	 * their own level only with manage_equal_rank, and on themselves only with the self_* permission for the rule
	 * (OTHERS: on themselves always, as before).
	 */
	AccountRules::eManageDenial TargetDenial(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, SlashCommandLevels::eTargetRule rule);

	// Why a command may not be used on someone, for the chat; empty when it may
	std::string TargetRefusal(AccountRules::eManageDenial denial, SlashCommandLevels::eTargetRule rule, std::string_view command);

	/**
	 * Whether the actor may use the command on the target (TargetDenial with the actor's current GM level and account).
	 * When not, tells whoever typed the command (sysAddr) why in the chat.
	 */
	bool MayActOn(Entity* actor, const SystemAddress& sysAddr, const CommandTarget& target, SlashCommandLevels::eTargetRule rule, std::string_view command);

	// The account of the player using a command (0 if unknown)
	uint32_t AccountOf(Entity* player);
};

namespace GMZeroCommands {
	void Help(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif // SLASHCOMMANDHANDLER_H
