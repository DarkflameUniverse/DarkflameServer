/*
 * Darkflame Universe
 * Copyright 2024
 */



#include "SlashCommandHandler.h"

#include <iomanip>
#include <ranges>

#include "DEVGMCommands.h"
#include "GMGreaterThanZeroCommands.h"
#include "GMZeroCommands.h"

#include "Amf3.h"
#include "Database.h"
#include "eChatMessageType.h"
#include "dServer.h"

namespace {
	std::map<std::string, Command> CommandInfos;
	std::map<std::string, Command> RegisteredCommands;
}

void SlashCommandHandler::RegisterCommand(Command command) {
	if (command.aliases.empty()) {
		LOG("Command %s has no aliases! Skipping!", command.help.c_str());
		return;
	}

	for (const auto& alias : command.aliases) {
		auto [_, success] = RegisteredCommands.try_emplace(alias, command);
		// Don't allow duplicate commands
		if (!success) {
			LOG_DEBUG("Command alias %s is already registered! Skipping!", alias.c_str());
			continue;
		}
	}
	CommandInfos[command.aliases[0]] = command;
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
		if (entity->GetGMLevel() >= commandHandle.requiredLevel) {
			if (commandHandle.requiredLevel > eGameMasterLevel::CIVILIAN) Database::Get()->InsertSlashCommandUsage(entity->GetObjectID(), input);
			commandHandle.handle(entity, sysAddr, args);
		} else if (entity->GetGMLevel() != eGameMasterLevel::CIVILIAN) {
			error = "You are not high enough GM level to use \"" + command + "\"";
		}
	} else if (entity->GetGMLevel() == eGameMasterLevel::CIVILIAN) {
		error = "Command " + command + " does not exist!";
	}

	if (!error.empty()) {
		GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(error));
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
			if (command.requiredLevel <= entity->GetGMLevel()) {
				accessibleCommands.emplace(commandName, command);
			}
		}

		size_t totalPages = (accessibleCommands.size() + pageSize - 1) / pageSize;

		if (page < 1 || page > totalPages) {
			feedback << "Invalid page number. Total pages: " << totalPages;
			GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedback.str()));
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
			GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedbackStr));
		}
		return;
	}

	auto it = std::ranges::find_if(CommandInfos, [&trimmedArgs](const auto& pair) {
		return std::ranges::find(pair.second.aliases, trimmedArgs) != pair.second.aliases.end();
		});

	if (it != CommandInfos.end() && entity->GetGMLevel() >= it->second.requiredLevel) {
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
		GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedbackStr));
	}
}

void SlashCommandHandler::SendAnnouncement(const std::string& title, const std::string& message) {
	AMFArrayValue args;

	args.Insert("title", title);
	args.Insert("message", message);

	GameMessages::SendUIMessageServerToAllClients("ToggleAnnounce", args);

	//Notify chat about it
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_ANNOUNCE);

	bitStream.Write<uint32_t>(title.size());
	for (auto character : title) {
		bitStream.Write<char>(character);
	}

	bitStream.Write<uint32_t>(message.size());
	for (auto character : message) {
		bitStream.Write<char>(character);
	}

	Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
}

void SlashCommandHandler::Startup() {
	// Register Dev Commands
	Command SetGMLevelCommand{
		.help = "Change the GM level of your character",
		.info = "Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands",
		.aliases = { "setgmlevel", "makegm", "gmlevel" },
		.handle = DEVGMCommands::SetGMLevel,
		.requiredLevel = eGameMasterLevel::CIVILIAN
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
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(KillCommand);

	Command MetricsCommand{
		.help = "Display server metrics",
		.info = "Prints some information about the server's performance",
		.aliases = { "metrics" },
		.handle = DEVGMCommands::Metrics,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(MetricsCommand);

	Command AnnounceCommand{
		.help = " Send and announcement",
		.info = "Sends an announcement. `/setanntitle` and `/setannmsg` must be called first to configure the announcement.",
		.aliases = { "announce" },
		.handle = DEVGMCommands::Announce,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AnnounceCommand);

	Command SetAnnTitleCommand{
		.help = "Sets the title of an announcement",
		.info = "Sets the title of an announcement. Use with `/setannmsg` and `/announce`",
		.aliases = { "setanntitle" },
		.handle = DEVGMCommands::SetAnnTitle,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetAnnTitleCommand);

	Command SetAnnMsgCommand{
		.help = "Sets the message of an announcement",
		.info = "Sets the message of an announcement. Use with `/setannmtitle` and `/announce`",
		.aliases = { "setannmsg" },
		.handle = DEVGMCommands::SetAnnMsg,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetAnnMsgCommand);

	Command ShutdownUniverseCommand{
		.help = "Sends a shutdown message to the master server",
		.info = "Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.",
		.aliases = { "shutdownuniverse" },
		.handle = DEVGMCommands::ShutdownUniverse
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
		.info = "Transfers you to the given zone by id and clone id. Add \"force\" to skip checking if the zone is accessible (this can softlock your character, though, if you e.g. try to teleport to Frostburgh).",
		.aliases = { "testmap", "tm" },
		.handle = DEVGMCommands::TestMap,
		.requiredLevel = eGameMasterLevel::FORUM_MODERATOR
	};
	RegisterCommand(TestMapCommand);

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
		.help = "Teleports you",
		.info = "Teleports you. If no Y is given, you are teleported to the height of the terrain or physics object at (x, z)",
		.aliases = { "teleport", "tele", "tp" },
		.handle = DEVGMCommands::Teleport,
		.requiredLevel = eGameMasterLevel::JUNIOR_DEVELOPER
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
		.info = "Finds the closest entity with the given component or LNV variable (ignoring players and racing cars), printing its ID, distance from the player, and whether it is sleeping, as well as the the IDs of all components the entity has. See detailed usage in the DLU docs",
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
		.requiredLevel = eGameMasterLevel::DEVELOPER
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
		.requiredLevel = eGameMasterLevel::DEVELOPER
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
		.requiredLevel = eGameMasterLevel::DEVELOPER
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

	// Register Greater Than Zero Commands

	Command KickCommand{
		.help = "Kicks the player off the server",
		.info = "Kicks the player off the server",
		.aliases = { "kick" },
		.handle = GMGreaterThanZeroCommands::Kick,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR
	};
	RegisterCommand(KickCommand);

	Command MailItemCommand{
		.help = "Mails an item to the given player",
		.info = "Mails an item to the given player. The mailed item has predetermined content. The sender name is set to \"Darkflame Universe\". The title of the message is \"Lost item\". The body of the message is \"This is a replacement item for one you lost\".",
		.aliases = { "mailitem" },
		.handle = GMGreaterThanZeroCommands::MailItem,
		.requiredLevel = eGameMasterLevel::MODERATOR
	};
	RegisterCommand(MailItemCommand);

	Command BanCommand{
		.help = "Bans a user from the server",
		.info = "Bans a user from the server",
		.aliases = { "ban" },
		.handle = GMGreaterThanZeroCommands::Ban,
		.requiredLevel = eGameMasterLevel::SENIOR_MODERATOR
	};
	RegisterCommand(BanCommand);

	Command ApprovePropertyCommand{
		.help = "Approves a property",
		.info = "Approves the property the player is currently visiting",
		.aliases = { "approveproperty" },
		.handle = GMGreaterThanZeroCommands::ApproveProperty,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	};
	RegisterCommand(ApprovePropertyCommand);

	Command MuteCommand{
		.help = "Mute a player",
		.info = "Mute player for the given amount of time. If no time is given, the mute is indefinite.",
		.aliases = { "mute" },
		.handle = GMGreaterThanZeroCommands::Mute,
		.requiredLevel = eGameMasterLevel::JUNIOR_DEVELOPER
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
		.info = "Toggles invisibility for the character, though it's currently a bit buggy. Requires nonzero GM Level for the character, but the account must have a GM level of 8",
		.aliases = { "gminvis" },
		.handle = GMGreaterThanZeroCommands::GmInvis,
		.requiredLevel = eGameMasterLevel::DEVELOPER
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
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR
	};
	RegisterCommand(ShowAllCommand);

	Command FindPlayerCommand{
		.help = "Find the World Server a player is in if they are online",
		.info = "Find the World Server a player is in if they are online",
		.aliases = { "findplayer" },
		.handle = GMGreaterThanZeroCommands::FindPlayer,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR
	};
	RegisterCommand(FindPlayerCommand);

	Command SpectateCommand{
		.help = "Spectate a player",
		.info = "Specify a player name to spectate. They must be in the same world as you. Leave blank to stop spectating",
		.aliases = { "spectate", "follow" },
		.handle = GMGreaterThanZeroCommands::Spectate,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR
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
		.aliases = { "requestmailcount" },
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
		.requiredLevel = eGameMasterLevel::CIVILIAN
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
}
