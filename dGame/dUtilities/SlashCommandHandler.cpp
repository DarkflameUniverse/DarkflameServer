/*
 * Darkflame Universe
 * Copyright 2024
 */



#include "SlashCommandHandler.h"
#include "ChatServerLink.h"
#include "ChatPackets.h"
#include "MasterPackets.h"
#include "master/CDClientReload.h"
#include "master/WorldFiles.h"
#include "WorldMigration.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <ranges>
#include <set>

#include "DEVGMCommands.h"
#include "GMGreaterThanZeroCommands.h"
#include "GMZeroCommands.h"
#include "GuildCommands.h"
#include "LiveEvents.h"

#include "Amf3.h"
#include "Database.h"
#include "MessageType/Chat.h"
#include "dServer.h"
#include "dConfig.h"
#include "SlashCommandLevels.h"
#include "Permissions.h"
#include "PermissionGrants.h"
#include "PermissionGrantsLoader.h"
#include "Character.h"
#include "ChatPackets.h"
#include "PlayerManager.h"
#include "User.h"
#include "eObjectBits.h"
#include "EffectsMessages.h"

namespace {
	// Each command once, by the first alias it got
	std::map<std::string, Command> CommandInfos;
	std::map<std::string, Command> RegisteredCommands;

	using Handler = void(*)(Entity*, const SystemAddress&, const std::string);
}

void SlashCommandHandler::RegisterCommand(Command command) {
	if (command.aliases.empty()) {
		LOG("Command %s has no aliases! Skipping!", command.help.c_str());
		return;
	}

	// The client acts on these by itself, so a level set here would change nothing
	if (const auto* target = command.handle.target<Handler>(); target && *target == GMZeroCommands::ClientHandled) {
		command.fixedLevel = true;
		command.clientHandled = true;
		if (command.levelNote.empty()) command.levelNote = "Handled by the game client";
	}
	if (!command.minLevel) command.minLevel = static_cast<eGameMasterLevel>(SlashCommandLevels::DefaultMinLevel(static_cast<uint8_t>(command.requiredLevel)));

	// Named after the first alias no other command has taken
	const auto primary = std::ranges::find_if(command.aliases, [](const auto& alias) { return !RegisteredCommands.contains(alias); });
	if (primary == command.aliases.end()) {
		LOG("Every alias of command %s is already registered! Skipping!", command.aliases[0].c_str());
		return;
	}
	command.name = SlashCommandLevels::SettingName(*primary);
	if (std::ranges::any_of(CommandInfos, [&command](const auto& info) { return info.second.name == command.name; })) {
		LOG("Command %s shares its settings name %s with another command", primary->c_str(), command.name.c_str());
	}

	for (const auto& alias : command.aliases) {
		auto [_, success] = RegisteredCommands.try_emplace(alias, command);
		// Don't allow duplicate commands
		if (!success) {
			LOG_DEBUG("Command alias %s is already registered! Skipping!", alias.c_str());
			continue;
		}
	}
	CommandInfos[*primary] = command;
}

eGameMasterLevel SlashCommandHandler::GetRequiredLevel(const Command& command) {
	if (command.fixedLevel) return command.requiredLevel;
	const auto defaultLevel = static_cast<uint8_t>(command.requiredLevel);
	const auto minLevel = static_cast<uint8_t>(command.minLevel.value_or(eGameMasterLevel::CIVILIAN));
	const std::string value = Game::config ? Game::config->GetValue(SlashCommandLevels::ConfigName(command.name)) : "";
	// Paired with a dashboard permission: its level, unless an explicit command level overrides it
	if (Permissions::Find(command.dashboardPermission)) {
		return static_cast<eGameMasterLevel>(SlashCommandLevels::ResolvePaired(defaultLevel, minLevel, false, Permissions::Level(command.dashboardPermission), value).level);
	}
	return static_cast<eGameMasterLevel>(SlashCommandLevels::Resolve(defaultLevel, minLevel, false, value));
}

const Command* SlashCommandHandler::FindCommand(const std::string& alias) {
	const auto it = RegisteredCommands.find(alias);
	return it == RegisteredCommands.end() ? nullptr : &it->second;
}

uint32_t SlashCommandHandler::AccountOf(Entity* player) {
	auto* character = player ? player->GetCharacter() : nullptr;
	auto* user = character ? character->GetParentUser() : nullptr;
	return user ? user->GetAccountID() : 0;
}

std::optional<CommandTarget> SlashCommandHandler::TargetOf(Entity* player) {
	auto* character = player ? player->GetCharacter() : nullptr;
	if (!character) return std::nullopt;
	auto* user = character->GetParentUser();
	CommandTarget target{ .accountId = user ? user->GetAccountID() : 0, .characterId = player->GetObjectID(), .name = character->GetName(), .entity = player };
	// The account's rank protects it even while its owner plays at a lower level (/setgmlevel)
	target.gmLevel = static_cast<uint8_t>(std::max(player->GetGMLevel(), user ? user->GetMaxGMLevel() : eGameMasterLevel::CIVILIAN));
	return target;
}

std::optional<CommandTarget> SlashCommandHandler::FindTarget(const std::string& name) {
	if (name.empty()) return std::nullopt;
	if (auto* player = PlayerManager::GetPlayer(name)) return TargetOf(player);
	const auto info = Database::Get()->GetCharacterInfo(name);
	if (!info || info->accountId == 0) return std::nullopt;
	CommandTarget target{ .accountId = info->accountId, .characterId = info->id, .name = info->name };
	GeneralUtils::SetBit(target.characterId, eObjectBits::CHARACTER);
	const auto account = Database::Get()->GetAccountById(info->accountId);
	// No account row: treat it as the highest level, so nobody below GM 9 acts on it by mistake
	target.gmLevel = account.contains("error") ? AccountRules::OPERATOR_LEVEL : account.value("gm_level", static_cast<uint8_t>(0));
	return target;
}

AccountRules::eManageDenial SlashCommandHandler::TargetDenial(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, SlashCommandLevels::eTargetRule rule,
	const PermissionGrants::Held* actorGrants) {
	using SlashCommandLevels::eTargetRule;
	using AccountRules::eAccountAction;
	const auto denial = [&](eAccountAction action) { return AccountRules::ManageDenialNow(actorLevel, actorAccountId, targetLevel, targetAccountId, action, nullptr, actorGrants); };
	switch (rule) {
	case eTargetRule::NONE: return AccountRules::eManageDenial::NONE;
	case eTargetRule::OTHERS:
		if (actorAccountId != 0 && actorAccountId == targetAccountId) return AccountRules::eManageDenial::NONE;
		return denial(eAccountAction::TOOLS);
	case eTargetRule::ITEMS: return denial(eAccountAction::ITEMS);
	case eTargetRule::MODERATION: return denial(eAccountAction::MODERATION);
	default: return denial(eAccountAction::TOOLS);
	}
}

const PermissionGrants::Held* SlashCommandHandler::GrantsOf(Entity* player) {
	auto* character = player ? player->GetCharacter() : nullptr;
	auto* user = character ? character->GetParentUser() : nullptr;
	if (!user) return nullptr;
	if (!user->GetGrants() || user->GetGrantsCharacter() != character->GetID()) {
		user->SetGrants(PermissionGrants::Load(user->GetAccountID(), character->GetID()), character->GetID());
	}
	return user->GetGrants().get();
}

PermissionGrants::Command SlashCommandHandler::GrantRules(const Command& command) {
	return { command.name, static_cast<uint8_t>(GetRequiredLevel(command)), static_cast<uint8_t>(command.minLevel.value_or(eGameMasterLevel::CIVILIAN)),
		command.fixedLevel, Permissions::Find(command.dashboardPermission) ? command.dashboardPermission : "" };
}

bool SlashCommandHandler::MayUse(Entity* player, const Command& command) {
	if (!player) return false;
	auto* character = player->GetCharacter();
	auto* user = character ? character->GetParentUser() : nullptr;
	const auto playerLevel = static_cast<uint8_t>(player->GetGMLevel());
	// Denies never apply to a GM 9 account, even while it plays at a lower level
	const auto accountLevel = std::max(playerLevel, static_cast<uint8_t>(user ? user->GetMaxGMLevel() : eGameMasterLevel::CIVILIAN));
	return PermissionGrants::MayUseCommand(playerLevel, accountLevel, GrantRules(command), GrantsOf(player), static_cast<int64_t>(std::time(nullptr)));
}

std::string SlashCommandHandler::TargetRefusal(AccountRules::eManageDenial denial, SlashCommandLevels::eTargetRule rule, std::string_view command) {
	if (denial == AccountRules::eManageDenial::NONE) return "";
	using SlashCommandLevels::eTargetRule;
	const auto action = rule == eTargetRule::ITEMS ? AccountRules::eAccountAction::ITEMS
		: rule == eTargetRule::MODERATION ? AccountRules::eAccountAction::MODERATION : AccountRules::eAccountAction::TOOLS;
	return "/" + std::string(command) + ": " + AccountRules::DenialMessage(denial, action);
}

bool SlashCommandHandler::MayActOn(Entity* actor, const SystemAddress& sysAddr, const CommandTarget& target, SlashCommandLevels::eTargetRule rule, std::string_view command) {
	if (!actor) return false;
	const auto denial = TargetDenial(static_cast<uint8_t>(actor->GetGMLevel()), AccountOf(actor), target.gmLevel, target.accountId, rule, GrantsOf(actor));
	if (denial == AccountRules::eManageDenial::NONE) return true;
	ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(TargetRefusal(denial, rule, command)));
	return false;
}

namespace {
	/**
	 * A command that now uses its dashboard permission's level, on a server that stored it before it did: keep the level it
	 * had as an override (a web value, like one set on the Permissions page, by UPGRADE_ACTOR) so the upgrade changes
	 * nothing by itself. The page shows it and can drop it. Returns whether one was stored.
	 */
	bool KeepLevelFromBeforePairing(const Command& command, const ISlashCommands::SlashCommand* before) {
		const auto* permission = Permissions::Find(command.dashboardPermission);
		if (!permission || !before) return false;
		const auto name = SlashCommandLevels::ConfigName(command.name);
		const std::string value = Game::config ? Game::config->GetValue(name) : "";
		const auto minLevel = static_cast<uint8_t>(command.minLevel.value_or(eGameMasterLevel::CIVILIAN));
		const auto permissionLevel = SlashCommandLevels::ResolvePaired(static_cast<uint8_t>(command.requiredLevel), minLevel, false, Permissions::Level(permission->key), "").level;
		const bool followedBefore = before->followsPermission && before->dashboardPermission == command.dashboardPermission;
		const auto keep = SlashCommandLevels::UpgradeOverride(true, followedBefore, command.fixedLevel, before->defaultLevel, minLevel, value, permissionLevel);
		if (!keep) return false;

		// Several worlds may start at once: the first one stores it
		const std::string file(SlashCommandLevels::CONFIG_FILE);
		for (const auto& row : Database::Get()->GetServerConfig({ file })) {
			if (row.name == name && row.webValue) return false;
		}
		const std::string actor(SlashCommandLevels::UPGRADE_ACTOR);
		Database::Get()->SetWebConfigValue(file, name, std::to_string(*keep), true, actor);
		const auto alias = "/" + command.aliases.front();
		const auto description = alias + " (" + command.name + "): kept at GM " + std::to_string(*keep) + "+ as an override, the level it had before it used the " +
			permission->key + " permission's level (GM " + std::to_string(permissionLevel) + "+). Drop the override on the Permissions page to follow the permission";
		try {
			Database::Get()->InsertAuditLog(0, actor, "change_command_level", description, 0, 0);
		} catch (const std::exception& ex) {
			LOG("Could not write the audit log entry for %s: %s", alias.c_str(), ex.what());
		}
		LOG("%s", description.c_str());
		return true;
	}
}

void SlashCommandHandler::ReportCommands() {
	try {
		std::map<std::string, ISlashCommands::SlashCommand> stored;
		for (auto& row : Database::Get()->GetSlashCommands()) stored[row.name] = std::move(row);

		std::set<std::string> current;
		uint32_t changed = 0;
		bool kept = false;
		for (const auto& [primary, command] : CommandInfos) {
			ISlashCommands::SlashCommand row{
				.name = command.name, .help = command.help, .info = command.info,
				.defaultLevel = static_cast<uint8_t>(command.requiredLevel),
				.minLevel = static_cast<uint8_t>(command.minLevel.value_or(command.requiredLevel)),
				.fixed = command.fixedLevel, .clientHandled = command.clientHandled, .note = command.levelNote, .dashboardPermission = command.dashboardPermission,
				.targetRule = std::string(SlashCommandLevels::TargetRuleName(command.targetRule)),
				.followsPermission = Permissions::Find(command.dashboardPermission) != nullptr
			};
			// Only the aliases that really run this command (another command may have one of them)
			for (const auto& alias : command.aliases) {
				const auto it = RegisteredCommands.find(alias);
				if (it != RegisteredCommands.end() && it->second.name == command.name) row.aliases.push_back(alias);
			}
			current.insert(row.name);
			const auto it = stored.find(row.name);
			if (it != stored.end() && it->second == row) continue;
			// Before the row says it follows the permission, so this happens once
			if (row.followsPermission && it != stored.end()) kept = KeepLevelFromBeforePairing(command, &it->second) || kept;
			Database::Get()->SetSlashCommand(row);
			changed++;
		}
		for (const auto& [name, row] : stored) {
			if (current.contains(name)) continue;
			Database::Get()->DeleteSlashCommand(name);
			changed++;
		}
		if (changed > 0) LOG("Updated %u slash command(s) for the dashboard", changed);
		// Use the levels kept from before straight away
		if (kept && Game::config) Game::config->ReloadConfig();
	} catch (const std::exception& ex) {
		// Before the migration has run there is no table yet
		LOG_DEBUG("Could not store the slash commands: %s", ex.what());
	}
}

void SlashCommandHandler::HandleChatCommand(const std::u16string& chat, Entity* entity, const SystemAddress& sysAddr) {
	auto input = GeneralUtils::UTF16ToWTF8(chat);
	if (input.empty() || input.front() != '/') return;
	const auto pos = input.find(' ');
	std::string command = input.substr(1, pos - 1);

	std::string args;
	// make sure the space exists and isn't the last character
	if (pos != std::string::npos && pos != input.size()) args = input.substr(input.find(' ') + 1);
	LOG_DEBUG("Handling command \"%s\" with args \"%s\"", command.c_str(), args.c_str());

	const auto commandItr = RegisteredCommands.find(command);
	std::string error;
	if (commandItr != RegisteredCommands.end()) {
		auto& [alias, commandHandle] = *commandItr;
		const auto requiredLevel = GetRequiredLevel(commandHandle);
		if (MayUse(entity, commandHandle)) {
			if (requiredLevel > eGameMasterLevel::CIVILIAN) Database::Get()->InsertSlashCommandUsage(entity->GetObjectID(), input);
			commandHandle.handle(entity, sysAddr, args);
		} else if (entity->GetGMLevel() >= requiredLevel) {
			// The level allows it, but a deny on the dashboard took it away
			error = "You may not use \"" + command + "\": it was taken away from you";
		} else if (entity->GetGMLevel() != eGameMasterLevel::CIVILIAN) {
			error = "You are not high enough GM level to use \"" + command + "\"";
		}
	} else if (entity->GetGMLevel() == eGameMasterLevel::CIVILIAN) {
		error = "Command " + command + " does not exist!";
	}

	if (!error.empty()) {
		GameMessages::SlashCommandTextFeedback(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(error)).SendToClient(entity->GetSystemAddress());
	}
}

void GMZeroCommands::Help(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	std::ostringstream feedback;
	constexpr size_t pageSize = 10;

	std::string trimmedArgs = args;
	trimmedArgs.erase(trimmedArgs.begin(), std::find_if_not(trimmedArgs.begin(), trimmedArgs.end(), [](unsigned char ch) {
		return std::isspace(ch);
		}));
	trimmedArgs.erase(std::find_if_not(trimmedArgs.rbegin(), trimmedArgs.rend(), [](unsigned char ch) {
		return std::isspace(ch);
		}).base(), trimmedArgs.end());

	std::optional<uint32_t> parsedPage = GeneralUtils::TryParse<uint32_t>(trimmedArgs);
	if (trimmedArgs.empty() || parsedPage.has_value()) {
		size_t page = parsedPage.value_or(1);

		std::map<std::string, Command> accessibleCommands;
		for (const auto& [commandName, command] : CommandInfos) {
			if (SlashCommandHandler::MayUse(entity, command)) {
				accessibleCommands.emplace(commandName, command);
			}
		}

		size_t totalPages = (accessibleCommands.size() + pageSize - 1) / pageSize;

		if (page < 1 || page > totalPages) {
			feedback << "Invalid page number. Total pages: " << totalPages;
			GameMessages::SlashCommandTextFeedback(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(feedback.str())).SendToClient(entity->GetSystemAddress());
			return;
		}

		auto it = accessibleCommands.begin();
		std::advance(it, (page - 1) * pageSize);
		size_t endIdx = std::min(page * pageSize, accessibleCommands.size());

		feedback << "----- Commands (Page " << page << " of " << totalPages << ") -----";
		for (size_t i = (page - 1) * pageSize; i < endIdx; ++i, ++it) {
			feedback << "\n/" << it->first << ": " << it->second.help;
		}

		const auto feedbackStr = feedback.str();
		if (!feedbackStr.empty()) {
			GameMessages::SlashCommandTextFeedback(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(feedbackStr)).SendToClient(entity->GetSystemAddress());
		}
		return;
	}

	const auto it = RegisteredCommands.find(trimmedArgs);
	if (it != RegisteredCommands.end() && SlashCommandHandler::MayUse(entity, it->second)) {
		const auto& command = it->second;
		feedback << "----- " << it->first << " Info -----\n";
		feedback << command.info << "\n";
		if (command.aliases.size() > 1) {
			feedback << "Aliases: ";
			for (size_t i = 0; i < command.aliases.size(); ++i) {
				if (i > 0) feedback << ", ";
				feedback << command.aliases[i];
			}
		}
	} else {
		feedback << "Command not found.";
	}

	const auto feedbackStr = feedback.str();
	if (!feedbackStr.empty()) {
		GameMessages::SlashCommandTextFeedback(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(feedbackStr)).SendToClient(entity->GetSystemAddress());
	}
}

void SlashCommandHandler::SendAnnouncement(const std::string& title, const std::string& message) {
	AMFArrayValue args;

	args.Insert("title", title);
	args.Insert("message", message);

	GameMessages::UIMessageServerToAllClients uiMessage;
	uiMessage.strMessageName = "ToggleAnnounce";
	uiMessage.args = std::move(args);
	uiMessage.Send(UNASSIGNED_SYSTEM_ADDRESS);

	//Notify chat about it
	ChatPackets::Announcement announcement;
	announcement.title = title;
	announcement.message = message;
	ChatServerLink::Send(announcement);
}

void SlashCommandHandler::Startup() {
	// Register Dev Commands
	Command SetGMLevelCommand{
		.help = "Change the GM level of your character",
		.info = "Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands",
		.aliases = { "setgmlevel", "makegm", "gmlevel" },
		.handle = DEVGMCommands::SetGMLevel,
		.requiredLevel = eGameMasterLevel::CIVILIAN,
		.fixedLevel = true,
		.levelNote = "Everyone keeps it so staff who lowered their GM level can raise it again (it never goes above the account's GM level)"
	};
	RegisterCommand(SetGMLevelCommand);

	Command ToggleNameplateCommand{
		.help = "Toggle the visibility of your nameplate. This must be enabled by a server admin to be used.",
		.info = "Turns the nameplate above your head that is visible to other players off and on. This must be enabled by a server admin to be used.",
		.aliases = { "togglenameplate", "tnp" },
		.handle = DEVGMCommands::ToggleNameplate,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(ToggleNameplateCommand);

	Command ToggleSkipCinematicsCommand{
		.help = "Toggle Skipping Cinematics",
		.info = "Skips mission and world load related cinematics",
		.aliases = { "toggleskipcinematics", "tsc" },
		.handle = DEVGMCommands::ToggleSkipCinematics,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(ToggleSkipCinematicsCommand);

	Command KillCommand{
		.help = "Smash a user",
		.info = "Smashes the character whom the given user is playing",
		.aliases = { "kill" },
		.handle = DEVGMCommands::Kill,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.targetRule = SlashCommandLevels::eTargetRule::TOOLS
	};
	RegisterCommand(KillCommand);

	Command MetricsCommand{
		.help = "Display server metrics",
		.info = "Prints some information about the server's performance",
		.aliases = { "metrics" },
		.handle = DEVGMCommands::Metrics,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "health_view"
	};
	RegisterCommand(MetricsCommand);

	Command AnnounceCommand{
		.help = " Send and announcement",
		.info = "Sends an announcement. `/setanntitle` and `/setannmsg` must be called first to configure the announcement.",
		.aliases = { "announce" },
		.handle = DEVGMCommands::Announce,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "server_announce"
	};
	RegisterCommand(AnnounceCommand);

	Command SetAnnTitleCommand{
		.help = "Sets the title of an announcement",
		.info = "Sets the title of an announcement. Use with `/setannmsg` and `/announce`",
		.aliases = { "setanntitle" },
		.handle = DEVGMCommands::SetAnnTitle,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "server_announce"
	};
	RegisterCommand(SetAnnTitleCommand);

	Command SetAnnMsgCommand{
		.help = "Sets the message of an announcement",
		.info = "Sets the message of an announcement. Use with `/setannmtitle` and `/announce`",
		.aliases = { "setannmsg" },
		.handle = DEVGMCommands::SetAnnMsg,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "server_announce"
	};
	RegisterCommand(SetAnnMsgCommand);

	Command ShutdownUniverseCommand{
		.help = "Sends a shutdown message to the master server",
		.info = "Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.",
		.aliases = { "shutdownuniverse" },
		.handle = DEVGMCommands::ShutdownUniverse,
		.dashboardPermission = "server_restart"
	};
	RegisterCommand(ShutdownUniverseCommand);

	Command SetMinifigCommand{
		.help = "Alters your player's minifig",
		.info = "Alters your player's minifig. Body part can be one of \"Eyebrows\", \"Eyes\", \"HairColor\", \"HairStyle\", \"Pants\", \"LeftHand\", \"Mouth\", \"RightHand\", \"Shirt\", or \"Hands\". Changing minifig parts could break the character so this command is limited to GMs.",
		.aliases = { "setminifig" },
		.handle = DEVGMCommands::SetMinifig,
		.requiredLevel = eGameMasterLevel::FORUM_MODERATOR
	};
	RegisterCommand(SetMinifigCommand);

	Command TestMapCommand{
		.help = "Transfers you to the given zone",
		.info = "Transfers you to the given zone by id and clone id and then spawns you at the specified spawn point if one was specified. Ignores instance-id for now.",
		.aliases = { "testmap", "tm" },
		.handle = DEVGMCommands::TestMap,
		.requiredLevel = eGameMasterLevel::FORUM_MODERATOR
	};
	RegisterCommand(TestMapCommand);

	Command ReprocessPropertyCommand{
		.help = "Make this property's models again and reload it",
		.info = "The UGC server makes every model placed on the property you are on again, with the current UGC settings or the processing options given (in any order: embree, hiprt or embree-gpu; off or oidn; native or toolbox-blender). Once they are made, everyone on the property is sent back into it, so their game loads the new meshes",
		.aliases = { "reprocessproperty", "reloadpropertymodels" },
		.handle = DEVGMCommands::ReprocessProperty,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReprocessPropertyCommand);

	Command ReportProxPhysCommand{
		.help = "Display proximity sensor info",
		.info = "Prints to console the position and radius of proximity sensors.",
		.aliases = { "reportproxphys" },
		.handle = DEVGMCommands::ReportProxPhys,
		.requiredLevel = eGameMasterLevel::OPERATOR
	};
	RegisterCommand(ReportProxPhysCommand);

	Command SpawnPhysicsVertsCommand{
		.help = "Spawns a 1x1 brick at all vertices of phantom physics objects",
		.info = "Spawns a 1x1 brick at all vertices of phantom physics objects",
		.aliases = { "spawnphysicsverts" },
		.handle = DEVGMCommands::SpawnPhysicsVerts,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnPhysicsVertsCommand);

	Command TeleportCommand{
		.help = "Teleports you to a position or a player to another player.",
		.info = "Teleports you. If no Y is given, you are teleported to the height of the terrain or physics object at (x, z). Any of the coordinates can use the syntax of an exact position (10.0), or a relative position (~+10.0). A ~ means use the current value of that axis as the base value. Addition or subtraction is supported (~+10) (~-10). If source player and target player are players that exist in the world, then the source player will be teleported to target player.",
		.aliases = { "teleport", "tele", "tp" },
		.handle = DEVGMCommands::Teleport,
		.requiredLevel = eGameMasterLevel::JUNIOR_DEVELOPER,
		.targetRule = SlashCommandLevels::eTargetRule::OTHERS
	};
	RegisterCommand(TeleportCommand);

	Command ActivateSpawnerCommand{
		.help = "Activates spawner by name",
		.info = "Activates spawner by name",
		.aliases = { "activatespawner" },
		.handle = DEVGMCommands::ActivateSpawner,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ActivateSpawnerCommand);

	Command AddMissionCommand{
		.help = "Accepts the mission, adding it to your journal.",
		.info = "Accepts the mission, adding it to your journal.",
		.aliases = { "addmission" },
		.handle = DEVGMCommands::AddMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AddMissionCommand);

	Command BoostCommand{
		.help = "Adds boost to a vehicle",
		.info = "Adds a passive boost action if you are in a vehicle. If time is given it will end after that amount of time",
		.aliases = { "boost" },
		.handle = DEVGMCommands::Boost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BoostCommand);

	Command UnboostCommand{
		.help = "Removes a passive vehicle boost",
		.info = "Removes a passive vehicle boost",
		.aliases = { "unboost" },
		.handle = DEVGMCommands::Unboost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(UnboostCommand);

	Command BuffCommand{
		.help = "Applies a buff",
		.info = "Applies a buff with the given id for the given number of seconds",
		.aliases = { "buff" },
		.handle = DEVGMCommands::Buff,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffCommand);

	Command BuffMeCommand{
		.help = "Sets health, armor, and imagination to 999",
		.info = "Sets health, armor, and imagination to 999",
		.aliases = { "buffme" },
		.handle = DEVGMCommands::BuffMe,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffMeCommand);

	Command BuffMedCommand{
		.help = "Sets health, armor, and imagination to 9",
		.info = "Sets health, armor, and imagination to 9",
		.aliases = { "buffmed" },
		.handle = DEVGMCommands::BuffMed,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffMedCommand);

	Command ClearFlagCommand{
		.help = "Clear a player flag",
		.info = "Removes the given health or inventory flag from your player. Equivalent of calling `/setflag off <flag id>`",
		.aliases = { "clearflag" },
		.handle = DEVGMCommands::ClearFlag,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ClearFlagCommand);

	Command CompleteMissionCommand{
		.help = "Completes the mission",
		.info = "Completes the mission, removing it from your journal",
		.aliases = { "completemission" },
		.handle = DEVGMCommands::CompleteMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CompleteMissionCommand);

	Command CreatePrivateCommand{
		.help = "Creates a private zone with password",
		.info = "Creates a private zone with password",
		.aliases = { "createprivate" },
		.handle = DEVGMCommands::CreatePrivate,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CreatePrivateCommand);

	Command DebugUiCommand{
		.help = "Toggle Debug UI",
		.info = "Toggle Debug UI",
		.aliases = { "debugui" },
		.handle = DEVGMCommands::DebugUi,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DebugUiCommand);

	Command DismountCommand{
		.help = "Dismounts you from the vehicle or mount",
		.info = "Dismounts you from the vehicle or mount",
		.aliases = { "dismount" },
		.handle = DEVGMCommands::Dismount,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DismountCommand);

	Command ReloadConfigCommand{
		.help = "Reload Server configs",
		.info = "Reloads the server with the new config values.",
		.aliases = { "reloadconfig", "reload-config" },
		.handle = DEVGMCommands::ReloadConfig,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReloadConfigCommand);

	Command ForceSaveCommand{
		.help = "Force save your player",
		.info = "While saving to database usually happens on regular intervals and when you disconnect from the server, this command saves your player's data to the database",
		.aliases = { "forcesave", "force-save" },
		.handle = DEVGMCommands::ForceSave,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ForceSaveCommand);

	Command FreecamCommand{
		.help = "Toggles freecam mode",
		.info = "Toggles freecam mode",
		.aliases = { "freecam" },
		.handle = DEVGMCommands::Freecam,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FreecamCommand);

	Command FreeMoneyCommand{
		.help = "Give yourself coins",
		.info = "Give yourself coins",
		.aliases = { "freemoney", "givemoney", "money", "givecoins", "coins"},
		.handle = DEVGMCommands::FreeMoney,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FreeMoneyCommand);

	Command GetNavmeshHeightCommand{
		.help = "Display the navmesh height",
		.info = "Display the navmesh height at your current position",
		.aliases = { "getnavmeshheight" },
		.handle = DEVGMCommands::GetNavmeshHeight,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GetNavmeshHeightCommand);

	Command GiveUScoreCommand{
		.help = "Gives uscore",
		.info = "Gives uscore",
		.aliases = { "giveuscore" },
		.handle = DEVGMCommands::GiveUScore,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GiveUScoreCommand);

	Command GmAddItemCommand{
		.help = "Give yourseld an item",
		.info = "Adds the given item to your inventory by id",
		.aliases = { "gmadditem", "give" },
		.handle = DEVGMCommands::GmAddItem,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GmAddItemCommand);

	Command InspectCommand{
		.help = "Inspect an object",
		.info = "Finds the closest entity with the given component or LNV variable (ignoring players and racing cars), printing its ID, distance from the player, and whether it is sleeping, as well as the IDs of all components the entity has. Use `localCharacter` or `zoneControl` to inspect your current character or the zone control object.",
		.aliases = { "inspect" },
		.handle = DEVGMCommands::Inspect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(InspectCommand);

	Command ListSpawnsCommand{
		.help = "List spawn points for players",
		.info = "Lists all the character spawn points in the zone. Additionally, this command will display the current scene that plays when the character lands in the next zone, if there is one.",
		.aliases = { "list-spawns", "listspawns" },
		.handle = DEVGMCommands::ListSpawns,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ListSpawnsCommand);

	Command LocRowCommand{
		.help = "Prints the your current position and rotation information to the console",
		.info = "Prints the your current position and rotation information to the console",
		.aliases = { "locrow" },
		.handle = DEVGMCommands::LocRow,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(LocRowCommand);

	Command LookupCommand{
		.help = "Lookup an object",
		.info = "Searches through the Objects table in the client SQLite database for items whose display name, name, or description contains the query. Query can be multiple words delimited by spaces.",
		.aliases = { "lookup" },
		.handle = DEVGMCommands::Lookup,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(LookupCommand);

	Command PlayAnimationCommand{
		.help = "Play an animation with given ID",
		.info = "Play an animation with given ID",
		.aliases = { "playanimation", "playanim" },
		.handle = DEVGMCommands::PlayAnimation,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayAnimationCommand);

	Command PlayEffectCommand{
		.help = "Plays an effect",
		.info = "Plays an effect",
		.aliases = { "playeffect" },
		.handle = DEVGMCommands::PlayEffect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayEffectCommand);

	Command PlayLvlFxCommand{
		.help = "Plays the level up animation on your character",
		.info = "Plays the level up animation on your character",
		.aliases = { "playlvlfx" },
		.handle = DEVGMCommands::PlayLvlFx,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayLvlFxCommand);

	Command PlayRebuildFxCommand{
		.help = "Plays the quickbuild animation on your character",
		.info = "Plays the quickbuild animation on your character",
		.aliases = { "playrebuildfx" },
		.handle = DEVGMCommands::PlayRebuildFx,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayRebuildFxCommand);

	Command PosCommand{
		.help = "Displays your current position in chat and in the console",
		.info = "Displays your current position in chat and in the console",
		.aliases = { "pos" },
		.handle = DEVGMCommands::Pos,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PosCommand);

	Command RefillStatsCommand{
		.help = "Refills health, armor, and imagination to their maximum level",
		.info = "Refills health, armor, and imagination to their maximum level",
		.aliases = { "refillstats" },
		.handle = DEVGMCommands::RefillStats,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RefillStatsCommand);

	Command ReforgeCommand{
		.help = "Reforges an item",
		.info = "Reforges an item",
		.aliases = { "reforge" },
		.handle = DEVGMCommands::Reforge,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReforgeCommand);

	Command ResetMissionCommand{
		.help = "Sets the state of the mission to accepted but not yet started",
		.info = "Sets the state of the mission to accepted but not yet started",
		.aliases = { "resetmission" },
		.handle = DEVGMCommands::ResetMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ResetMissionCommand);

	Command RotCommand{
		.help = "Displays your current rotation in chat and in the console",
		.info = "Displays your current rotation in chat and in the console",
		.aliases = { "rot" },
		.handle = DEVGMCommands::Rot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RotCommand);

	Command RunMacroCommand{
		.help = "Run a macro",
		.info = "Runs any command macro found in `./res/macros/`",
		.aliases = { "runmacro" },
		.handle = DEVGMCommands::RunMacro,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RunMacroCommand);

	Command SetControlSchemeCommand{
		.help = "Sets the character control scheme to the specified number",
		.info = "Sets the character control scheme to the specified number",
		.aliases = { "setcontrolscheme" },
		.handle = DEVGMCommands::SetControlScheme,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetControlSchemeCommand);

	Command SetCurrencyCommand{
		.help = "Sets your coins",
		.info = "Sets your coins",
		.aliases = { "setcurrency", "setcoins" },
		.handle = DEVGMCommands::SetCurrency,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetCurrencyCommand);

	Command SetFlagCommand{
		.help = "Set a player flag",
		.info = "Sets the given inventory or health flag to the given value, where value can be one of \"on\" or \"off\". If no value is given, by default this adds the flag to your character (equivalent of calling `/setflag on <flag id>`)",
		.aliases = { "setflag" },
		.handle = DEVGMCommands::SetFlag,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetFlagCommand);

	Command SetInventorySizeCommand{
		.help = "Set your inventory size",
		.info = "Sets your inventory size to the given size. If `inventory` is provided, the number or string will be used to set that inventory to the requested size",
		.aliases = { "setinventorysize", "setinvsize", "setinvensize" },
		.handle = DEVGMCommands::SetInventorySize,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetInventorySizeCommand);

	Command SetUiStateCommand{
		.help = "Changes UI state",
		.info = "Changes UI state",
		.aliases = { "setuistate" },
		.handle = DEVGMCommands::SetUiState,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetUiStateCommand);

	Command SpawnCommand{
		.help = "Spawns an object at your location by id",
		.info = "Spawns an object at your location by id",
		.aliases = { "spawn" },
		.handle = DEVGMCommands::Spawn,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnCommand);

	Command SpawnGroupCommand{
		.help = "",
		.info = "",
		.aliases = { "spawngroup" },
		.handle = DEVGMCommands::SpawnGroup,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnGroupCommand);

	Command SpeedBoostCommand{
		.help = "Set the players speed multiplier",
		.info = "Sets the speed multiplier to the given amount. `/speedboost 1.5` will set the speed multiplier to 1.5x the normal speed",
		.aliases = { "speedboost" },
		.handle = DEVGMCommands::SpeedBoost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpeedBoostCommand);

	Command StartCelebrationCommand{
		.help = "Starts a celebration effect on your character",
		.info = "Starts a celebration effect on your character",
		.aliases = { "startcelebration" },
		.handle = DEVGMCommands::StartCelebration,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(StartCelebrationCommand);

	Command StopEffectCommand{
		.help = "Stops the given effect",
		.info = "Stops the given effect",
		.aliases = { "stopeffect" },
		.handle = DEVGMCommands::StopEffect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(StopEffectCommand);

	Command ToggleCommand{
		.help = "Toggles UI state",
		.info = "Toggles UI state",
		.aliases = { "toggle" },
		.handle = DEVGMCommands::Toggle,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ToggleCommand);

	Command TpAllCommand{
		.help = "Teleports all characters to your current position",
		.info = "Teleports all characters to your current position",
		.aliases = { "tpall" },
		.handle = DEVGMCommands::TpAll,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.targetRule = SlashCommandLevels::eTargetRule::OTHERS
	};
	RegisterCommand(TpAllCommand);

	Command TriggerSpawnerCommand{
		.help = "Triggers spawner by name",
		.info = "Triggers spawner by name",
		.aliases = { "triggerspawner" },
		.handle = DEVGMCommands::TriggerSpawner,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TriggerSpawnerCommand);

	Command UnlockEmoteCommand{
		.help = "Unlocks for your character the emote of the given id",
		.info = "Unlocks for your character the emote of the given id",
		.aliases = { "unlock-emote", "unlockemote" },
		.handle = DEVGMCommands::UnlockEmote,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(UnlockEmoteCommand);

	Command SetLevelCommand{
		.help = "Set player level",
		.info = "Sets the using entities level to the requested level.  Takes an optional parameter of an in-game players username to set the level of",
		.aliases = { "setlevel" },
		.handle = DEVGMCommands::SetLevel,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.targetRule = SlashCommandLevels::eTargetRule::OTHERS
	};
	RegisterCommand(SetLevelCommand);

	Command SetSkillSlotCommand{
		.help = "Set an action slot to a specific skill",
		.info = "Set an action slot to a specific skill",
		.aliases = { "setskillslot" },
		.handle = DEVGMCommands::SetSkillSlot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetSkillSlotCommand);

	Command SetFactionCommand{
		.help = "Set the players faction",
		.info = "Clears the users current factions and sets it",
		.aliases = { "setfaction" },
		.handle = DEVGMCommands::SetFaction,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetFactionCommand);

	Command AddFactionCommand{
		.help = "Add the faction to the users list of factions",
		.info = "Add the faction to the users list of factions",
		.aliases = { "addfaction" },
		.handle = DEVGMCommands::AddFaction,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AddFactionCommand);

	Command GetFactionsCommand{
		.help = "Shows the player's factions",
		.info = "Shows the player's factions",
		.aliases = { "getfactions" },
		.handle = DEVGMCommands::GetFactions,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GetFactionsCommand);

	Command SetRewardCodeCommand{
		.help = "Set a reward code for your account",
		.info = "Sets the rewardcode for the account you are logged into if it's a valid rewardcode, See cdclient table `RewardCodes`",
		.aliases = { "setrewardcode" },
		.handle = DEVGMCommands::SetRewardCode,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetRewardCodeCommand);

	Command CrashCommand{
		.help = "Crash the server",
		.info = "Crashes the server",
		.aliases = { "crash", "pumpkin" },
		.handle = DEVGMCommands::Crash,
		.requiredLevel = eGameMasterLevel::OPERATOR
	};
	RegisterCommand(CrashCommand);

	Command RollLootCommand{
		.help = "Simulate loot rolls",
		.info = "Given a `loot matrix index`, look for `item id` in that matrix `amount` times and print to the chat box statistics of rolling that loot matrix.",
		.aliases = { "rollloot", "roll-loot" },
		.handle = DEVGMCommands::RollLoot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RollLootCommand);

	Command CastSkillCommand{
		.help = "Casts the skill as the player",
		.info = "Casts the skill as the player",
		.aliases = { "castskill" },
		.handle = DEVGMCommands::CastSkill,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CastSkillCommand);

	Command DeleteInvenCommand{
		.help = "Delete all items from a specified inventory",
		.info = "Delete all items from a specified inventory",
		.aliases = { "deleteinven" },
		.handle = DEVGMCommands::DeleteInven,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DeleteInvenCommand);

	Command ExecuteCommand{
		.help = "Execute commands with modified context (Minecraft-style)",
		.info = "Execute commands as different entities or from different positions. Usage: /execute <subcommand> ... run <command>. Subcommands: as <entity>, at <entity>, positioned <x> <y> <z>",
		.aliases = { "execute", "exec" },
		.handle = DEVGMCommands::Execute,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.minLevel = eGameMasterLevel::DEVELOPER,
		.levelNote = "Runs commands as another player, with that player's GM level, so it can't go below its default",
		.targetRule = SlashCommandLevels::eTargetRule::OTHERS
	};
	RegisterCommand(ExecuteCommand);

	// Register Greater Than Zero Commands

	Command KickCommand{
		.help = "Kicks the player off the server",
		.info = "Kicks the player off the server",
		.aliases = { "kick" },
		.handle = GMGreaterThanZeroCommands::Kick,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR,
		.dashboardPermission = "accounts_kick",
		.targetRule = SlashCommandLevels::eTargetRule::TOOLS
	};
	RegisterCommand(KickCommand);

	Command MailItemCommand{
		.help = "Mails an item to the given player",
		.info = "Mails an item to the given player. The mailed item has predetermined content. The sender name is set to \"Darkflame Universe\". The title of the message is \"Lost item\". The body of the message is \"This is a replacement item for one you lost\".",
		.aliases = { "mailitem" },
		.handle = GMGreaterThanZeroCommands::MailItem,
		.requiredLevel = eGameMasterLevel::MODERATOR,
		.dashboardPermission = "mail_items",
		.targetRule = SlashCommandLevels::eTargetRule::ITEMS
	};
	RegisterCommand(MailItemCommand);

	Command BanCommand{
		.help = "Bans a user from the server",
		.info = "Bans a user from the server",
		.aliases = { "ban" },
		.handle = GMGreaterThanZeroCommands::Ban,
		.requiredLevel = eGameMasterLevel::SENIOR_MODERATOR,
		.dashboardPermission = "accounts_ban",
		.targetRule = SlashCommandLevels::eTargetRule::MODERATION
	};
	RegisterCommand(BanCommand);

	Command ApprovePropertyCommand{
		.help = "Approves a property",
		.info = "Approves the property the player is currently visiting",
		.aliases = { "approveproperty" },
		.handle = GMGreaterThanZeroCommands::ApproveProperty,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR,
		.dashboardPermission = "moderate_properties"
	};
	RegisterCommand(ApprovePropertyCommand);

	Command MuteCommand{
		.help = "Mute a player",
		.info = "Mute player for the given amount of time. If no time is given, the mute is indefinite.",
		.aliases = { "mute" },
		.handle = GMGreaterThanZeroCommands::Mute,
		.requiredLevel = eGameMasterLevel::JUNIOR_DEVELOPER,
		.dashboardPermission = "accounts_mute",
		.targetRule = SlashCommandLevels::eTargetRule::MODERATION
	};
	RegisterCommand(MuteCommand);

	Command FlyCommand{
		.help = "Toggle flying",
		.info = "Toggles your flying state with an optional parameter for the speed scale.",
		.aliases = { "fly" },
		.handle = GMGreaterThanZeroCommands::Fly,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FlyCommand);

	Command AttackImmuneCommand{
		.help = "Make yourself immune to attacks",
		.info = "Sets the character's immunity to basic attacks state, where value can be one of \"1\", to make yourself immune to basic attack damage, or \"0\" to undo",
		.aliases = { "attackimmune" },
		.handle = GMGreaterThanZeroCommands::AttackImmune,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AttackImmuneCommand);

	Command GmImmuneCommand{
		.help = "Sets the character's GMImmune state",
		.info = "Sets the character's GMImmune state, where value can be one of \"1\", to make yourself immune to damage, or \"0\" to undo",
		.aliases = { "gmimmune" },
		.handle = GMGreaterThanZeroCommands::GmImmune,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GmImmuneCommand);

	Command GmInvisCommand{
		.help = "Toggles invisibility for the character",
		.info = "Toggles invisibility for the character, making them invisible to other players and lower GM levels",
		.aliases = { "gminvis" },
		.handle = GMGreaterThanZeroCommands::GmInvis,
		.requiredLevel = eGameMasterLevel::FORUM_MODERATOR
	};
	RegisterCommand(GmInvisCommand);

	Command SetNameCommand{

		.help = "Sets a temporary name for your player",
		.info = "Sets a temporary name for your player. The name resets when you log out",
		.aliases = { "setname" },
		.handle = GMGreaterThanZeroCommands::SetName,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetNameCommand);

	Command TitleCommand{
		.help = "Give your character a title",
		.info = "Temporarily appends your player's name with \" - &#60;title&#62;\". This resets when you log out",
		.aliases = { "title" },
		.handle = GMGreaterThanZeroCommands::Title,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TitleCommand);

	Command ShowAllCommand{
		.help = "Show all online players across World Servers",
		.info = "Usage: /showall (displayZoneData: Default 1) (displayIndividualPlayers: Default 1)",
		.aliases = { "showall" },
		.handle = GMGreaterThanZeroCommands::ShowAll,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR,
		.dashboardPermission = "players_view"
	};
	RegisterCommand(ShowAllCommand);

	Command FindPlayerCommand{
		.help = "Find the World Server a player is in if they are online",
		.info = "Find the World Server a player is in if they are online",
		.aliases = { "findplayer" },
		.handle = GMGreaterThanZeroCommands::FindPlayer,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR,
		.dashboardPermission = "players_view"
	};
	RegisterCommand(FindPlayerCommand);

	Command SpectateCommand{
		.help = "Spectate a player",
		.info = "Specify a player name to spectate. They must be in the same world as you. Leave blank to stop spectating",
		.aliases = { "spectate", "follow" },
		.handle = GMGreaterThanZeroCommands::Spectate,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR,
		.dashboardPermission = "players_view"
	};
	RegisterCommand(SpectateCommand);

	// Register GM Zero Commands

	Command HelpCommand{
		.help = "Display command info",
		.info = "If a command is given, display detailed info on that command. Otherwise display a list of commands with short descriptions.",
		.aliases = { "help", "h"},
		.handle = GMZeroCommands::Help,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(HelpCommand);

	Command CreditsCommand{
		.help = "Displays DLU Credits",
		.info = "Displays the names of the people behind Darkflame Universe.",
		.aliases = { "credits" },
		.handle = GMZeroCommands::Credits,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(CreditsCommand);

	Command InfoCommand{
		.help = "Displays server info",
		.info = "Displays server info to the user, including where to find the server's source code",
		.aliases = { "info" },
		.handle = GMZeroCommands::Info,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(InfoCommand);

	Command DieCommand{
		.help = "Smashes the player",
		.info = "Smashes the player as if they were killed by something",
		.aliases = { "die" },
		.handle = GMZeroCommands::Die,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(DieCommand);

	Command PingCommand{
		.help = "Displays your average ping.",
		.info = "Displays your average ping. If the `-l` flag is used, the latest ping is displayed.",
		.aliases = { "ping" },
		.handle = GMZeroCommands::Ping,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(PingCommand);

	Command PvpCommand{
		.help = "Toggle your PVP flag",
		.info = "Toggle your PVP flag",
		.aliases = { "pvp" },
		.handle = GMZeroCommands::Pvp,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(PvpCommand);

	Command RequestMailCountCommand{
		.help = "Gets the players mail count",
		.info = "Sends notification with number of unread messages in the player's mailbox",
		.aliases = { "requestmailcount", "checkmail" },
		.handle = GMZeroCommands::RequestMailCount,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(RequestMailCountCommand);

	Command WhoCommand{
		.help = "Displays all players on the instance",
		.info = "Displays all players on the instance",
		.aliases = { "who" },
		.handle = GMZeroCommands::Who,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(WhoCommand);

	Command FixStatsCommand{
		.help = "Resets skills, buffs, and destroyables",
		.info = "Resets skills, buffs, and destroyables",
		.aliases = { "fix-stats" },
		.handle = GMZeroCommands::FixStats,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(FixStatsCommand);

	Command JoinCommand{
		.help = "Join a private zone",
		.info = "Join a private zone with given password",
		.aliases = { "join" },
		.handle = GMZeroCommands::Join,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(JoinCommand);

	Command LeaveZoneCommand{
		.help = "Leave an instanced zone",
		.info = "If you are in an instanced zone, transfers you to the closest main world. For example, if you are in an instance of Avant Gardens Survival or the Spider Queen Battle, you are sent to Avant Gardens. If you are in the Battle of Nimbus Station, you are sent to Nimbus Station.",
		.aliases = { "leave-zone", "leavezone" },
		.handle = GMZeroCommands::LeaveZone,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(LeaveZoneCommand);

	Command ResurrectCommand{
		.help = "Resurrects the player",
		.info = "Resurrects the player",
		.aliases = { "resurrect" },
		.handle = GMZeroCommands::Resurrect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ResurrectCommand);

	Command InstanceInfoCommand{
		.help = "Display LWOZoneID info for the current zone",
		.info = "Display LWOZoneID info for the current zone",
		.aliases = { "instanceinfo" },
		.handle = GMZeroCommands::InstanceInfo,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(InstanceInfoCommand);

	Command ServerUptimeCommand{
		.help = "Display the time the current world server has been active",
		.info = "Display the time the current world server has been active",
		.aliases = { "uptime" },
		.handle = GMZeroCommands::ServerUptime,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "health_view"
	};
	RegisterCommand(ServerUptimeCommand);

	// Guilds (docs/Guilds.md): the client has no guild commands; its guild chat tab sends /g
	Command guildChatCommand{
		.help = "Send a message to your guild.",
		.info = "Send a message to your guild. The guild chat tab sends this.",
		.aliases = { "g", "guild" },
		.handle = GuildCommands::Chat,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildChatCommand);

	Command guildCreateCommand{
		.help = "Open the window to create a guild.",
		.info = "Open the window to create a guild (live opened it from the Guild Master NPC).",
		.aliases = { "guildcreate", "createguild" },
		.handle = GuildCommands::OpenCreateBox,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildCreateCommand);

	Command guildKickCommand{
		.help = "Remove a player from your guild.",
		.info = "Remove a player from your guild: the leader can remove anyone, officers veterans and recruits.",
		.aliases = { "gkick", "guildkick" },
		.handle = GuildCommands::Kick,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildKickCommand);

	Command guildRankCommand{
		.help = "Set a guild member's rank: /grank <name> <officer|veteran|recruit>",
		.info = "Set a guild member's rank. The leader sets any rank; officers move members between veteran and recruit.",
		.aliases = { "grank", "guildrank" },
		.handle = GuildCommands::Rank,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildRankCommand);

	Command guildLeaderCommand{
		.help = "Hand your guild over to another member: /gleader <name>",
		.info = "Make another member the guild's leader; you become an officer.",
		.aliases = { "gleader", "guildleader" },
		.handle = GuildCommands::Leader,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildLeaderCommand);

	Command guildDisbandCommand{
		.help = "Disband your guild: /gdisband confirm",
		.info = "The leader removes every member and the guild itself.",
		.aliases = { "gdisband", "guilddisband" },
		.handle = GuildCommands::Disband,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(guildDisbandCommand);

	//Commands that are handled by the client

	Command faqCommand{
		.help = "Show the LU FAQ Page",
		.info = "Show the LU FAQ Page",
		.aliases = {"faq","faqs"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(faqCommand);

	Command teamChatCommand{
		.help = "Send a message to your teammates.",
		.info = "Send a message to your teammates.",
		.aliases = {"team","t"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(teamChatCommand);

	Command showStoreCommand{
		.help = "Show the LEGO shop page.",
		.info = "Show the LEGO shop page.",
		.aliases = {"shop","store"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(showStoreCommand);

	Command minigamesCommand{
		.help = "Show the LEGO minigames page!",
		.info = "Show the LEGO minigames page!",
		.aliases = {"minigames"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(minigamesCommand);

	Command forumsCommand{
		.help = "Show the LU Forums!",
		.info = "Show the LU Forums!",
		.aliases = {"forums"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(forumsCommand);

	Command exitGameCommand{
		.help = "Exit to desktop",
		.info = "Exit to desktop",
		.aliases = {"exit","quit"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(exitGameCommand);

	Command thumbsUpCommand{
		.help = "Oh, yeah!",
		.info = "Oh, yeah!",
		.aliases = {"thumb","thumbs","thumbsup"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(thumbsUpCommand);

	Command victoryCommand{
		.help = "Victory!",
		.info = "Victory!",
		.aliases = {"victory!"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(victoryCommand);

	Command backflipCommand{
		.help = "Do a flip!",
		.info = "Do a flip!",
		.aliases = {"backflip"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(backflipCommand);

	Command clapCommand{
		.help = "A round of applause!",
		.info = "A round of applause!",
		.aliases = {"clap"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(clapCommand);

	Command logoutCharacterCommand{
		.help = "Returns you to the character select screen.",
		.info = "Returns you to the character select screen.",
		.aliases = {"camp","logoutcharacter"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(logoutCharacterCommand);

	Command sayCommand{
		.help = "Say something outloud so that everyone can hear you",
		.info = "Say something outloud so that everyone can hear you",
		.aliases = {"s","say"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(sayCommand);

	Command whisperCommand{
		.help = "Send a private message to another player.",
		.info = "Send a private message to another player.",
		.aliases = {"tell","w","whisper"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(whisperCommand);

	Command locationCommand{
		.help = "Output your current location on the map to the chat box.",
		.info = "Output your current location on the map to the chat box.",
		.aliases = {"loc","locate","location"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(locationCommand);

	Command logoutCommand{
		.help = "Returns you to the login screen.",
		.info = "Returns you to the login screen.",
		.aliases = {"logout","logoutaccount"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(logoutCommand);

	Command shrugCommand{
		.help = "I dunno...",
		.info = "I dunno...",
		.aliases = {"shrug"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(shrugCommand);

	Command leaveTeamCommand{
		.help = "Leave your current team.",
		.info = "Leave your current team.",
		.aliases = {"leave","leaveteam","teamleave","tleave"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(leaveTeamCommand);

	Command teamLootTypeCommand{
		.help = "[rr|ffa] Set the loot for your current team (round-robin/free for all).",
		.info = "[rr|ffa] Set the loot for your current team (round-robin/free for all).",
		.aliases = {"setloot","teamsetloot","tloot","tsetloot"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(teamLootTypeCommand);

	Command removeFriendCommand{
		.help = "[name] Removes a player from your friends list.",
		.info = "[name] Removes a player from your friends list.",
		.aliases = {"removefriend"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(removeFriendCommand);

	Command yesCommand{
		.help = "Aye aye, captain!",
		.info = "Aye aye, captain!",
		.aliases = {"yes"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(yesCommand);

	Command teamInviteCommand{
		.help = "[name] Invite a player to your team.",
		.info = "[name] Invite a player to your team.",
		.aliases = {"invite","inviteteam","teaminvite","tinvite"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(teamInviteCommand);

	Command danceCommand{
		.help = "Dance 'til you can't dance no more.",
		.info = "Dance 'til you can't dance no more.",
		.aliases = {"dance"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(danceCommand);

	Command sighCommand{
		.help = "Another day, another brick.",
		.info = "Another day, another brick.",
		.aliases = {"sigh"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(sighCommand);

	Command recommendedOptionsCommand{
		.help = "Sets the recommended performance options in the cfg file",
		.info = "Sets the recommended performance options in the cfg file",
		.aliases = {"recommendedperfoptions"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(recommendedOptionsCommand);

	Command setTeamLeaderCommand{
		.help = "[name] Set the leader for your current team.",
		.info = "[name] Set the leader for your current team.",
		.aliases = {"leader","setleader","teamsetleader","tleader","tsetleader"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(setTeamLeaderCommand);

	Command cringeCommand{
		.help = "I don't even want to talk about it...",
		.info = "I don't even want to talk about it...",
		.aliases = {"cringe"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(cringeCommand);

	Command talkCommand{
		.help = "Jibber Jabber",
		.info = "Jibber Jabber",
		.aliases = {"talk"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(talkCommand);

	Command cancelQueueCommand{
		.help = "Cancel Your position in the queue if you are in one.",
		.info = "Cancel Your position in the queue if you are in one.",
		.aliases = {"cancelqueue"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(cancelQueueCommand);

	Command lowPerformanceCommand{
		.help = "Sets the default low-spec performance options in the cfg file",
		.info = "Sets the default low-spec performance options in the cfg file",
		.aliases = {"perfoptionslow"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(lowPerformanceCommand);

	Command kickFromTeamCommand{
		.help = "[name] Kick a player from your current team.",
		.info = "[name] Kick a player from your current team.",
		.aliases = {"kick","kickplayer","teamkickplayer","tkick","tkickplayer"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(kickFromTeamCommand);

	Command thanksCommand{
		.help = "Express your gratitude for another.",
		.info = "Express your gratitude for another.",
		.aliases = {"thanks"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(thanksCommand);

	Command waveCommand{
		.help = "Wave to other players.",
		.info = "Wave to other players.",
		.aliases = {"wave"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(waveCommand);

	Command whyCommand{
		.help = "Why|!?!!",
		.info = "Why|!?!!",
		.aliases = {"why"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(whyCommand);

	Command midPerformanceCommand{
		.help = "Sets the default medium-spec performance options in the cfg file",
		.info = "Sets the default medium-spec performance options in the cfg file",
		.aliases = {"perfoptionsmid"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(midPerformanceCommand);

	Command highPerformanceCommand{
		.help = "Sets the default high-spec performance options in the cfg file",
		.info = "Sets the default high-spec performance options in the cfg file",
		.aliases = {"perfoptionshigh"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(highPerformanceCommand);

	Command gaspCommand{
		.help = "Oh my goodness!",
		.info = "Oh my goodness!",
		.aliases = {"gasp"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(gaspCommand);

	Command ignoreCommand{
		.help = "[name] Add a player to your ignore list.",
		.info = "[name] Add a player to your ignore list.",
		.aliases = {"addignore"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(ignoreCommand);

	Command addFriendCommand{
		.help = "[name] Add a player to your friends list.",
		.info = "[name] Add a player to your friends list.",
		.aliases = {"addfriend"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(addFriendCommand);

	Command cryCommand{
		.help = "Show everyone your 'Aw' face.",
		.info = "Show everyone your 'Aw' face.",
		.aliases = {"cry"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(cryCommand);

	Command giggleCommand{
		.help = "A good little chuckle",
		.info = "A good little chuckle",
		.aliases = {"giggle"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(giggleCommand);

	Command saluteCommand{
		.help = "For those about to build...",
		.info = "For those about to build...",
		.aliases = {"salute"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(saluteCommand);

	Command removeIgnoreCommand{
		.help = "[name] Removes a player from your ignore list.",
		.info = "[name] Removes a player from your ignore list.",
		.aliases = {"removeIgnore"},
		.handle = GMZeroCommands::ClientHandled,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(removeIgnoreCommand);

	Command command{
		.help = "Shuts this world down",
		.info = "Shuts this world down",
		.aliases = {"shutdown"},
		.handle = DEVGMCommands::Shutdown,
		.requiredLevel = eGameMasterLevel::DEVELOPER,
		.dashboardPermission = "worlds_manage"
	};
	RegisterCommand(command);

	RegisterCommand({
		.help = "Turns all players' pvp mode on",
		.info = "Turns all players' pvp mode on",
		.aliases = {"barfight"},
		.handle = DEVGMCommands::Barfight,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	});

	RegisterCommand({
		.help = "Despawns an object by id",
		.info = "Despawns an object by id",
		.aliases = {"despawn"},
		.handle = DEVGMCommands::Despawn,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	});
	RegisterCommand({
		.help = "Moves everyone here to a fresh instance of this zone",
		.info = "Starts a fresh instance of this zone (from the world server binary on disk now, so an update takes effect), moves everyone here to it with the game's Mythran maintenance warning and a short loading screen, then shuts this instance down. Usage: /replaceinstance [warn seconds, 0-300, default 10] [seamless (experimental: no loading screen)]",
		.aliases = {"replaceinstance"},
		.handle = WorldMigration::ReplaceInstanceCommand,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	});

	RegisterCommand({
		.help = "Moves everyone here into another instance of this zone",
		.info = "Moves everyone here into another running instance of this zone (0 or nothing picks the best fit), with the game's Mythran maintenance warning and a short loading screen, then shuts this instance down. Usage: /mergeinstance [target instance] [warn seconds, 0-300, default 10] [seamless (experimental: no loading screen)]",
		.aliases = {"mergeinstance"},
		.handle = WorldMigration::MergeInstanceCommand,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	});

	RegisterCommand({
		.help = "Moves the whole server onto the binaries on disk now",
		.info = "A live update: with a new build in place, master restarts the UGC, auth and chat servers, replaces every world instance with a new one and moves its players there (with the game's Mythran maintenance warning and a short loading screen), then restarts the dashboard. Master itself keeps running. Usage: /liveupdate [start [warn seconds, 0-300] | cancel | status]",
		.aliases = {"liveupdate"},
		.handle = WorldMigration::LiveUpdateCommand,
		.requiredLevel = eGameMasterLevel::OPERATOR,
		.dashboardPermission = "server_live_update"
	});

	RegisterCommand({
		.help = "Reloads the client's cdclient.fdb on every server",
		.info = "Master copies the client's cdclient.fdb (if it changed), makes a new CDServer.sqlite from it with the cdserver migrations, and every server switches to them between frames. What is already spawned keeps what it loaded; what is made from now on reads the new data. Master also does this by itself a few seconds after the file changes. Usage: /reloadcdclient",
		.aliases = {"reloadcdclient"},
		.handle = [](Entity* entity, const SystemAddress& sysAddr, const std::string) {
			CDClientReload request;
			request.requesterId = entity->GetObjectID();
			MasterPackets::SendToMaster(request);
			ChatPackets::SendSystemMessage(sysAddr, u"Asked master to check the client's cdclient.fdb; the server logs say what changed.");
		},
		.requiredLevel = eGameMasterLevel::OPERATOR,
		.dashboardPermission = "cdclient_reload"
	});

	RegisterCommand({
		.help = "Replaces every instance of a zone with one on the zone files on disk now",
		.info = "Master starts a new instance of the zone (properties keep their clone, private instances their password) that loads the .luz, .lvl, triggers, terrain and navmesh on disk now, moves the players over (the Mythran shift, or the seamless mode with world_reload_seamless=1) and stops the old instances; empty ones are just stopped. Master also does this by itself a few seconds after a file a world loaded changes (world_watch_seconds). Usage: /reloadworld [zone, default this one] [warn seconds, 0-300, default 10]",
		.aliases = {"reloadworld"},
		.handle = [](Entity* entity, const SystemAddress& sysAddr, const std::string args) {
			const auto words = GeneralUtils::SplitString(args, ' ');
			WorldReloadRequest request;
			request.zoneId = Game::server->GetZoneID();
			if (!words.empty() && !words[0].empty()) {
				const auto zone = GeneralUtils::TryParse<uint32_t>(words[0]);
				if (!zone || *zone == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Usage: /reloadworld [zone] [warn seconds, 0-300]");
					return;
				}
				request.zoneId = *zone;
			}
			if (words.size() > 1) {
				const auto warn = GeneralUtils::TryParse<uint16_t>(words[1]);
				if (!warn || *warn > InstanceMigrationRequest::MAX_WARN_SECONDS) {
					ChatPackets::SendSystemMessage(sysAddr, u"Usage: /reloadworld [zone] [warn seconds, 0-300]");
					return;
				}
				request.warnSeconds = *warn;
			}
			if (request.zoneId == 0) {
				ChatPackets::SendSystemMessage(sysAddr, u"Character selection loads no zone; name one: /reloadworld <zone>");
				return;
			}
			request.requesterId = entity->GetObjectID();
			if (auto* character = entity->GetCharacter()) request.requestedBy = character->GetName();
			MasterPackets::SendToMaster(request);
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16("Asked master to reload zone " + std::to_string(request.zoneId) +
				"; you are told how each instance's move goes, and the server log says what was stopped."));
		},
		.requiredLevel = eGameMasterLevel::OPERATOR,
		.dashboardPermission = "world_reload"
	});

	RegisterCommand({
		.help = "[claim] Community challenges and live events",
		.info = "Shows the server-wide community challenges running now, how far along they are and what you added, and the live events in this world. Also gives you the coins waiting from challenges you helped complete",
		.aliases = {"challenge", "challenges"},
		.handle = LiveEvents::ChallengeCommand,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	});
}
