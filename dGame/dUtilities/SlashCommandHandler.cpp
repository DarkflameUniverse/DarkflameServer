/*
 * Darkflame Universe
 * Copyright 2024
 */

#include "SlashCommandHandler.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <exception>
#include "dZoneManager.h"

#include "Metrics.hpp"

#include "User.h"
#include "UserManager.h"
#include "BitStream.h"
#include "dCommonVars.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Logger.h"
#include "WorldPackets.h"
#include "GameMessages.h"
#include "CDClientDatabase.h"
#include "ZoneInstanceManager.h"
#include "ControllablePhysicsComponent.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "ChatPackets.h"
#include "InventoryComponent.h"
#include "Game.h"
#include "CharacterComponent.h"
#include "Database.h"
#include "DestroyableComponent.h"
#include "dServer.h"
#include "MissionComponent.h"
#include "Mail.h"
#include "dpWorld.h"
#include "Item.h"
#include "PropertyManagementComponent.h"
#include "BitStreamUtils.h"
#include "Loot.h"
#include "EntityInfo.h"
#include "LUTriggers.h"
#include "PhantomPhysicsComponent.h"
#include "ProximityMonitorComponent.h"
#include "dpShapeSphere.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "BuffComponent.h"
#include "SkillComponent.h"
#include "VanityUtilities.h"
#include "ScriptedActivityComponent.h"
#include "LevelProgressionComponent.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "dConfig.h"
#include "eBubbleType.h"
#include "Amf3.h"
#include "MovingPlatformComponent.h"
#include "eMissionState.h"
#include "TriggerComponent.h"
#include "eServerDisconnectIdentifiers.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "RenderComponent.h"
#include "eControlScheme.h"
#include "eConnectionType.h"
#include "eChatMessageType.h"
#include "eMasterMessageType.h"
#include "PlayerManager.h"

#include "CDRewardCodesTable.h"
#include "CDObjectsTable.h"
#include "CDZoneTableTable.h"
#include "ePlayerFlag.h"
#include "dNavMesh.h"

namespace {
	std::vector<Command> CommandInfos;
	std::map<std::string, Command> RegisteredCommands;
}

void SlashCommandHandler::RegisterCommand(Command command) {
	if (command.aliases.empty()) return;

	LOG_DEBUG("Registering SlashCommand: %s", command.aliases[0].c_str());
	std::vector<std::string> toRemove;
	for (auto& alias : command.aliases) {
		if (alias.empty()) continue;
		if (RegisteredCommands.contains(alias)){
			LOG_DEBUG("Command alias %s is already registered! Skipping!", alias.c_str());
			// denote it to be removed
			toRemove.push_back(alias);
			continue;
		}
		RegisteredCommands.emplace(make_pair(alias, command));
	}
	// Actually remove the duplicate aliases here
	for (auto& removing : toRemove) {
		command.aliases.erase(std::find(std::cbegin(command.aliases), std::cend(command.aliases), removing));
	}
	CommandInfos.push_back(command);
};

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
		.help = "Toggle the visibility of your nameplate",
		.info = "Turns the nameplate above your head that is visible to other players off and on",
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

	Command KillCommand {
		.help = "Smash a user",
		.info = "Smashes the character whom the given user is playing",
		.aliases = { "kill" },
		.handle = DEVGMCommands::Kill,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(KillCommand);

	Command MetricsCommand {
		.help = "Display server metrics",
		.info = "Prints some information about the server's performance",
		.aliases = { "metrics" },
		.handle = DEVGMCommands::Metrics,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(MetricsCommand);

	Command AnnounceCommand {
		.help = " Send and announcement",
		.info = "Sends an announcement. `/setanntitle` and `/setannmsg` must be called first to configure the announcement.",
		.aliases = { "announce" },
		.handle = DEVGMCommands::Announce,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AnnounceCommand);

	Command SetAnnTitleCommand {
		.help = "Sets the title of an announcement",
		.info = "Sets the title of an announcement. Use with `/setannmsg` and `/announce`",
		.aliases = { "setanntitle" },
		.handle = DEVGMCommands::SetAnnTitle,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetAnnTitleCommand);

	Command SetAnnMsgCommand {
		.help = "Sets the message of an announcement",
		.info = "Sets the message of an announcement. Use with `/setannmtitle` and `/announce`",
		.aliases = { "setannmsg" },
		.handle = DEVGMCommands::SetAnnMsg,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetAnnMsgCommand);

	Command ShutdownUniverseCommand {
		.help = "Sends a shutdown message to the master server",
		.info = "Sends a shutdown message to the master server. This will send an announcement to all players that the universe will shut down in 10 minutes.",
		.aliases = { "shutdownuniverse" },
		.handle = DEVGMCommands::ShutdownUniverse
	};
	RegisterCommand(ShutdownUniverseCommand);

	Command SetMinifigCommand {
		.help = "",
		.info = "",
		.aliases = { "setminifig" },
		.handle = DEVGMCommands::SetMinifig,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetMinifigCommand);

	Command TestMapCommand {
		.help = "",
		.info = "",
		.aliases = { "testmap", "tm" },
		.handle = DEVGMCommands::TestMap,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TestMapCommand);

	Command ReportProxPhysCommand {
		.help = "",
		.info = "",
		.aliases = { "reportproxphys" },
		.handle = DEVGMCommands::ReportProxPhys,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReportProxPhysCommand);

	Command SpawnPhysicsVertsCommand {
		.help = "",
		.info = "",
		.aliases = { "spawnphysicsverts" },
		.handle = DEVGMCommands::SpawnPhysicsVerts,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnPhysicsVertsCommand);

	Command TeleportCommand {
		.help = "",
		.info = "",
		.aliases = { "teleport", "tele", "tp" },
		.handle = DEVGMCommands::Teleport,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TeleportCommand);

	Command ActivateSpawnerCommand {
		.help = "",
		.info = "",
		.aliases = { "activatespawner" },
		.handle = DEVGMCommands::ActivateSpawner,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ActivateSpawnerCommand);

	Command AddMissionCommand {
		.help = "",
		.info = "",
		.aliases = { "addmission" },
		.handle = DEVGMCommands::AddMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AddMissionCommand);

	Command BoostCommand {
		.help = "",
		.info = "",
		.aliases = { "boost" },
		.handle = DEVGMCommands::Boost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BoostCommand);

	Command UnboostCommand {
		.help = "",
		.info = "",
		.aliases = { "unboost" },
		.handle = DEVGMCommands::Unboost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(UnboostCommand);

	Command BuffCommand {
		.help = "",
		.info = "",
		.aliases = { "buff" },
		.handle = DEVGMCommands::Buff,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffCommand);

	Command BuffMeCommand {
		.help = "",
		.info = "",
		.aliases = { "buffme" },
		.handle = DEVGMCommands::BuffMe,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffMeCommand);

	Command BuffMedCommand {
		.help = "",
		.info = "",
		.aliases = { "buffmed" },
		.handle = DEVGMCommands::BuffMed,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(BuffMedCommand);

	Command ClearFlagCommand {
		.help = "",
		.info = "",
		.aliases = { "clearflag" },
		.handle = DEVGMCommands::ClearFlag,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ClearFlagCommand);

	Command CompleteMissionCommand {
		.help = "",
		.info = "",
		.aliases = { "completemission" },
		.handle = DEVGMCommands::CompleteMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CompleteMissionCommand);

	Command CreatePrivateCommand {
		.help = "",
		.info = "",
		.aliases = { "createprivate" },
		.handle = DEVGMCommands::CreatePrivate,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CreatePrivateCommand);

	Command DebugUiCommand {
		.help = "",
		.info = "",
		.aliases = { "debugui" },
		.handle = DEVGMCommands::DebugUi,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DebugUiCommand);

	Command DismountCommand {
		.help = "",
		.info = "",
		.aliases = { "dismount" },
		.handle = DEVGMCommands::Dismount,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DismountCommand);

	Command ReloadConfigCommand {
		.help = "",
		.info = "",
		.aliases = { "reloadconfig", "reload-config" },
		.handle = DEVGMCommands::ReloadConfig,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReloadConfigCommand);

	Command ForceSaveCommand {
		.help = "",
		.info = "",
		.aliases = { "forcesave", "force-save" },
		.handle = DEVGMCommands::ForceSave,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ForceSaveCommand);

	Command FreecamCommand {
		.help = "",
		.info = "",
		.aliases = { "freecam" },
		.handle = DEVGMCommands::Freecam,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FreecamCommand);

	Command FreeMoneyCommand {
		.help = "",
		.info = "",
		.aliases = { "freemoney" },
		.handle = DEVGMCommands::FreeMoney,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FreeMoneyCommand);

	Command GetNavmeshHeightCommand {
		.help = "",
		.info = "",
		.aliases = { "getnavmeshheight" },
		.handle = DEVGMCommands::GetNavmeshHeight,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GetNavmeshHeightCommand);

	Command GiveUScoreCommand {
		.help = "",
		.info = "",
		.aliases = { "giveuscore" },
		.handle = DEVGMCommands::GiveUScore,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GiveUScoreCommand);

	Command GmAddItemCommand {
		.help = "",
		.info = "",
		.aliases = { "gmadditem", "give" },
		.handle = DEVGMCommands::GmAddItem,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GmAddItemCommand);

	Command InspectCommand {
		.help = "",
		.info = "",
		.aliases = { "inspect" },
		.handle = DEVGMCommands::Inspect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(InspectCommand);

	Command ListSpawnsCommand {
		.help = "",
		.info = "",
		.aliases = { "list-spawns", "listspawns" },
		.handle = DEVGMCommands::ListSpawns,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ListSpawnsCommand);

	Command LocRowCommand {
		.help = "",
		.info = "",
		.aliases = { "locrow" },
		.handle = DEVGMCommands::LocRow,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(LocRowCommand);

	Command LookupCommand {
		.help = "",
		.info = "",
		.aliases = { "lookup" },
		.handle = DEVGMCommands::Lookup,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(LookupCommand);

	Command PlayAnimationCommand {
		.help = "",
		.info = "",
		.aliases = { "playanimation", "playanim" },
		.handle = DEVGMCommands::PlayAnimation,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayAnimationCommand);

	Command PlayEffectCommand {
		.help = "",
		.info = "",
		.aliases = { "playeffect" },
		.handle = DEVGMCommands::PlayEffect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayEffectCommand);

	Command PlayLvlFxCommand {
		.help = "",
		.info = "",
		.aliases = { "playlvlfx" },
		.handle = DEVGMCommands::PlayLvlFx,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayLvlFxCommand);

	Command PlayRebuildFxCommand {
		.help = "",
		.info = "",
		.aliases = { "playrebuildfx" },
		.handle = DEVGMCommands::PlayRebuildFx,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PlayRebuildFxCommand);

	Command PosCommand {
		.help = "",
		.info = "",
		.aliases = { "pos" },
		.handle = DEVGMCommands::Pos,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(PosCommand);

	Command RefillStatsCommand {
		.help = "",
		.info = "",
		.aliases = { "refillstats" },
		.handle = DEVGMCommands::RefillStats,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RefillStatsCommand);

	Command ReforgeCommand {
		.help = "",
		.info = "",
		.aliases = { "reforge" },
		.handle = DEVGMCommands::Reforge,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ReforgeCommand);

	Command ResetMissionCommand {
		.help = "",
		.info = "",
		.aliases = { "resetmission" },
		.handle = DEVGMCommands::ResetMission,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ResetMissionCommand);

	Command RotCommand {
		.help = "",
		.info = "",
		.aliases = { "rot" },
		.handle = DEVGMCommands::Rot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RotCommand);

	Command RunMacroCommand {
		.help = "",
		.info = "",
		.aliases = { "runmacro" },
		.handle = DEVGMCommands::RunMacro,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RunMacroCommand);

	Command SetControlSchemeCommand {
		.help = "",
		.info = "",
		.aliases = { "setcontrolscheme" },
		.handle = DEVGMCommands::SetControlScheme,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetControlSchemeCommand);

	Command SetCurrencyCommand {
		.help = "",
		.info = "",
		.aliases = { "setcurrency" },
		.handle = DEVGMCommands::SetCurrency,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetCurrencyCommand);

	Command SetFlagCommand {
		.help = "",
		.info = "",
		.aliases = { "setflag" },
		.handle = DEVGMCommands::SetFlag,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetFlagCommand);

	Command SetInventorySizeCommand {
		.help = "",
		.info = "",
		.aliases = { "setinventorysize" },
		.handle = DEVGMCommands::SetInventorySize,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetInventorySizeCommand);

	Command SetUiStateCommand {
		.help = "",
		.info = "",
		.aliases = { "setuistate" },
		.handle = DEVGMCommands::SetUiState,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetUiStateCommand);

	Command SpawnCommand {
		.help = "",
		.info = "",
		.aliases = { "spawn" },
		.handle = DEVGMCommands::Spawn,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnCommand);

	Command SpawnGroupCommand {
		.help = "",
		.info = "",
		.aliases = { "spawngroup" },
		.handle = DEVGMCommands::SpawnGroup,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpawnGroupCommand);

	Command SpeedBoostCommand {
		.help = "",
		.info = "",
		.aliases = { "speedboost" },
		.handle = DEVGMCommands::SpeedBoost,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SpeedBoostCommand);

	Command StartCelebrationCommand {
		.help = "",
		.info = "",
		.aliases = { "startcelebration" },
		.handle = DEVGMCommands::StartCelebration,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(StartCelebrationCommand);

	Command StopEffectCommand {
		.help = "",
		.info = "",
		.aliases = { "stopeffect" },
		.handle = DEVGMCommands::StopEffect,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(StopEffectCommand);

	Command ToggleCommand {
		.help = "",
		.info = "",
		.aliases = { "toggle" },
		.handle = DEVGMCommands::Toggle,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(ToggleCommand);

	Command TpAllCommand {
		.help = "",
		.info = "",
		.aliases = { "tpall" },
		.handle = DEVGMCommands::TpAll,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TpAllCommand);

	Command TriggerSpawnerCommand {
		.help = "",
		.info = "",
		.aliases = { "triggerspawner" },
		.handle = DEVGMCommands::TriggerSpawner,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TriggerSpawnerCommand);

	Command UnlockEmoteCommand {
		.help = "",
		.info = "",
		.aliases = { "unlock-emote", "unlockemote" },
		.handle = DEVGMCommands::UnlockEmote,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(UnlockEmoteCommand);

	Command SetLevelCommand {
		.help = "",
		.info = "",
		.aliases = { "setlevel" },
		.handle = DEVGMCommands::SetLevel,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetLevelCommand);

	Command SetSkillSlotCommand {
		.help = "",
		.info = "",
		.aliases = { "setskillslot" },
		.handle = DEVGMCommands::SetSkillSlot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetSkillSlotCommand);

	Command SetFactionCommand {
		.help = "",
		.info = "",
		.aliases = { "setfaction" },
		.handle = DEVGMCommands::SetFaction,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetFactionCommand);

	Command AddFactionCommand {
		.help = "",
		.info = "",
		.aliases = { "addfaction" },
		.handle = DEVGMCommands::AddFaction,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AddFactionCommand);

	Command GetFactionsCommand {
		.help = "",
		.info = "",
		.aliases = { "getfactions" },
		.handle = DEVGMCommands::GetFactions,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GetFactionsCommand);

	Command SetRewardCodeCommand {
		.help = "",
		.info = "",
		.aliases = { "setrewardcode" },
		.handle = DEVGMCommands::SetRewardCode,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetRewardCodeCommand);

	Command CrashCommand {
		.help = "",
		.info = "",
		.aliases = { "crash", "pumpkin"  },
		.handle = DEVGMCommands::Crash,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CrashCommand);

	Command RollLootCommand {
		.help = "",
		.info = "",
		.aliases = { "rollloot", "roll-loot" },
		.handle = DEVGMCommands::RollLoot,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(RollLootCommand);

	Command CastSkillCommand {
		.help = "",
		.info = "",
		.aliases = { "castskill" },
		.handle = DEVGMCommands::CastSkill,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(CastSkillCommand);

	Command DeleteInvenCommand {
		.help = "",
		.info = "",
		.aliases = { "deleteinven" },
		.handle = DEVGMCommands::CastSkill,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(DeleteInvenCommand);

	// Register Greater Than Zero Commands

	Command KickCommand {
		.help = "Kicks the player off the server",
		.info = "Kicks the player off the server",
		.aliases = { "kick" },
		.handle = GMGreaterThanZeroCommands::Kick,
		.requiredLevel = eGameMasterLevel::JUNIOR_MODERATOR
	};
	RegisterCommand(KickCommand);

	Command MailItemCommand {
		.help = "Mails an item to the given player",
		.info = "Mails an item to the given player. The mailed item has predetermined content. The sender name is set to \"Darkflame Universe\". The title of the message is \"Lost item\". The body of the message is \"This is a replacement item for one you lost\".",
		.aliases = { "mailitem" },
		.handle = GMGreaterThanZeroCommands::MailItem,
		.requiredLevel = eGameMasterLevel::MODERATOR
	};
	RegisterCommand(MailItemCommand);

	Command BanCommand {
		.help = "Bans a user from the server",
		.info = "Bans a user from the server",
		.aliases = { "ban" },
		.handle = GMGreaterThanZeroCommands::Ban,
		.requiredLevel = eGameMasterLevel::SENIOR_MODERATOR
	};
	RegisterCommand(BanCommand);

	Command ApprovePropertyCommand {
		.help = "Approves a property",
		.info = "Approves the property the player is currently visiting",
		.aliases = { "approveproperty" },
		.handle = GMGreaterThanZeroCommands::ApproveProperty,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	};
	RegisterCommand(ApprovePropertyCommand);

	Command MuteCommand {
		.help = "Mute a player",
		.info = "Mute player for the given amount of time. If no time is given, the mute is indefinite.",
		.aliases = { "mute" },
		.handle = GMGreaterThanZeroCommands::Mute,
		.requiredLevel = eGameMasterLevel::JUNIOR_DEVELOPER
	};
	RegisterCommand(MuteCommand);

	Command FlyCommand {
		.help = "Toggle flying",
		.info = "Toggles your flying state with an optional parameter for the speed scale.",
		.aliases = { "fly" },
		.handle = GMGreaterThanZeroCommands::Fly,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(FlyCommand);

	Command AttackImmuneCommand {
		.help = "Make yourself immune to attacks",
		.info = "Sets the character's immunity to basic attacks state, where value can be one of \"1\", to make yourself immune to basic attack damage, or \"0\" to undo",
		.aliases = { "attackimmune" },
		.handle = GMGreaterThanZeroCommands::AttackImmune,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(AttackImmuneCommand);

	Command GmImmuneCommand {
		.help = "Sets the character's GMImmune state",
		.info = "Sets the character's GMImmune state, where value can be one of \"1\", to make yourself immune to damage, or \"0\" to undo",
		.aliases = { "gmimmune" },
		.handle = GMGreaterThanZeroCommands::GmImmune,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GmImmuneCommand);

	Command GmInvisCommand {
		.help = "Toggles invisibility for the character",
		.info = "Toggles invisibility for the character, though it's currently a bit buggy. Requires nonzero GM Level for the character, but the account must have a GM level of 8",
		.aliases = { "gminvis" },
		.handle = GMGreaterThanZeroCommands::GmInvis,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(GmInvisCommand);

	Command SetNameCommand {

		.help = "Sets a temporary name for your player",
		.info = "Sets a temporary name for your player. The name resets when you log out",
		.aliases = { "setname" },
		.handle = GMGreaterThanZeroCommands::SetName,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(SetNameCommand);

	Command TitleCommand {
		.help = "Give your character a title",
		.info = "Temporarily appends your player's name with \" - &#60;title&#62;\". This resets when you log out",
		.aliases = { "title" },
		.handle = GMGreaterThanZeroCommands::Title,
		.requiredLevel = eGameMasterLevel::DEVELOPER
	};
	RegisterCommand(TitleCommand);


	// Register GM Zero Commands
	Command HelpCommand{
		.help = "Display command info",
		.info = "If a command is given, display detailed info on that command. Otherwise display a list of commands with short desctiptions.",
		.aliases = { "help", "h"},
		.handle = GMZeroCommands::Help,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(HelpCommand);

	Command CreditsCommand {
		.help = "Displays DLU Credits",
		.info = "Displays the names of the people behind Darkflame Universe.",
		.aliases = { "credits" },
		.handle = GMZeroCommands::Credits,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(CreditsCommand);

	Command InfoCommand {
		.help = "Displays server info",
		.info = "Displays server info to the user, including where to find the server's source code",
		.aliases = { "info" },
		.handle = GMZeroCommands::Info,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(InfoCommand);

	Command DieCommand {
		.help = "Smashes the player",
		.info = "Smashes the player as if they were killed by something",
		.aliases = { "die" },
		.handle = GMZeroCommands::Die,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(DieCommand);

	Command PingCommand {
		.help = "Displays your average ping.",
		.info = "Displays your average ping. If the `-l` flag is used, the latest ping is displayed.",
		.aliases = { "ping" },
		.handle = GMZeroCommands::Ping,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(PingCommand);

	Command PvpCommand {
		.help = "Toggle your PVP flag",
		.info = "Toggle your PVP flag",
		.aliases = { "pvp" },
		.handle = GMZeroCommands::Pvp,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(PvpCommand);

	Command RequestMailCountCommand {
		.help = "Gets the players mail count",
		.info = "Sends notification with number of unread messages in the player's mailbox",
		.aliases = { "requestmailcount" },
		.handle = GMZeroCommands::RequestMailCount,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(RequestMailCountCommand);

	Command WhoCommand {
		.help = "Displays all players on the instance",
		.info = "Displays all players on the instance",
		.aliases = { "who" },
		.handle = GMZeroCommands::Who,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(WhoCommand);

	Command FixStatsCommand {
		.help = "Resets skills, buffs, and destroyables",
		.info = "Resets skills, buffs, and destroyables",
		.aliases = { "fix-stats" },
		.handle = GMZeroCommands::FixStats,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(FixStatsCommand);

	Command JoinCommand {
		.help = "Join a private zone",
		.info = "Join a private zone with given password",
		.aliases = { "join" },
		.handle = GMZeroCommands::Join,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(JoinCommand);

	Command LeaveZoneCommand {
		.help = "Leave an instanced zone",
		.info = "If you are in an instanced zone, transfers you to the closest main world. For example, if you are in an instance of Avant Gardens Survival or the Spider Queen Battle, you are sent to Avant Gardens. If you are in the Battle of Nimbus Station, you are sent to Nimbus Station.",
		.aliases = { "leave-zone", "leavezone" },
		.handle = GMZeroCommands::LeaveZone,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(LeaveZoneCommand);

	Command ResurrectCommand {
		.help = "Resurrects the player",
		.info = "Resurrects the player",
		.aliases = { "resurrect" },
		.handle = GMZeroCommands::Resurrect,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(ResurrectCommand);

	Command InstanceInfoCommand {
		.help = "",
		.info = "",
		.aliases = { "instanceinfo" },
		.handle = GMZeroCommands::InstanceInfo,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	RegisterCommand(InstanceInfoCommand);

}

void SlashCommandHandler::HandleChatCommand(const std::u16string& chat, Entity* entity, const SystemAddress& sysAddr) {
	auto input = GeneralUtils::UTF16ToWTF8(chat);
	if (input.empty() || input.front() != u'/') return;
	std::string command = input.substr(1, input.find(' ') - 1);

	std::string args = input.substr(input.find(' ') + 1, std::string::npos);
	if (args.front() == '/') args.clear();
	LOG("Handling command \"%s\" with args \"%s\"", command.c_str(), args.c_str());

	if (RegisteredCommands.contains(command)) {
		if (entity->GetGMLevel() >= RegisteredCommands[command].requiredLevel) {
			Database::Get()->InsertSlashCommandUsage(entity->GetObjectID(), input);
			RegisteredCommands[command].handle(entity, sysAddr, args);
			return;
		} else {
			// We don't need to tell normies they aren't high enough level
			if (entity->GetGMLevel() == eGameMasterLevel::CIVILIAN) return;
			std::ostringstream feedback;
			feedback << "You are not high enough GM level to use " << std::quoted(command) << "";
			GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedback.str()));
		}
	} else {
		// We don't need to tell normies commands don't exist
		if (entity->GetGMLevel() == eGameMasterLevel::CIVILIAN) return;
		std::ostringstream feedback;
		feedback << "Command " << std::quoted(command) << " does not exist!";
		GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedback.str()));
	}
}

namespace GMZeroCommands {
	void Help(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (args.empty()) {
			std::ostringstream helpMessage;
			helpMessage << "----- Commands -----\n*";
			for (auto& command : CommandInfos) {
				// TODO: Limit displaying commands based on GM level they require
				if (command.requiredLevel > entity->GetGMLevel()) continue;
				helpMessage << "/" << command.aliases[0] << ": " << command.help << "\n*";
			}
			GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(helpMessage.str().substr(0, helpMessage.str().size() - 2)));
		} else {
			bool foundCommand = false;
			for (auto& command : CommandInfos) {
				if (std::find(command.aliases.begin(), command.aliases.end(), args) != command.aliases.end()) {
					foundCommand = true;
					if (entity->GetGMLevel() >= command.requiredLevel) {
						std::ostringstream commandDetails;
						commandDetails << "----- " << command.aliases[0] << " -----n*";
						commandDetails << command.info << "\n*";
						if (command.aliases.size() > 1) {
							commandDetails << "Aliases: ";
							std::copy(command.aliases.begin(), command.aliases.end(), std::ostream_iterator<std::string>(commandDetails, ", "));
						}
						GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(commandDetails.str().substr(0, commandDetails.str().size() - 2)));
					}
				} 
			}

			if (!foundCommand && entity->GetGMLevel() > eGameMasterLevel::CIVILIAN) {
				std::ostringstream feedback;
				feedback << "Command " << std::quoted(args) << " does not exist!";
				GameMessages::SendSlashCommandFeedbackText(entity, GeneralUtils::ASCIIToUTF16(feedback.str()));
			}
		}
	}

	void Pvp(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* character = entity->GetComponent<CharacterComponent>();

		if (character == nullptr) {
			LOG("Failed to find character component!");
			return;
		}

		character->SetPvpEnabled(!character->GetPvpEnabled());
		Game::entityManager->SerializeEntity(entity);

		std::stringstream message;
		message << character->GetName() << " changed their PVP flag to " << std::to_string(character->GetPvpEnabled()) << "!";

		ChatPackets::SendSystemMessage(UNASSIGNED_SYSTEM_ADDRESS, GeneralUtils::UTF8ToUTF16(message.str()), true);
	}

	void Who(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Players in this instance: (" + GeneralUtils::to_u16string(PlayerManager::GetAllPlayers().size()) + u")"
		);

		for (auto* player : PlayerManager::GetAllPlayers()) {
			const auto& name = player->GetCharacter()->GetName();

			ChatPackets::SendSystemMessage(
				sysAddr,
				GeneralUtils::UTF8ToUTF16(player == entity ? name + " (you)" : name)
			);
		}
	}

	void Ping(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (!args.empty() && args.starts_with("-l")) {
			std::stringstream message;
			message << "Your latest ping: " << std::to_string(Game::server->GetLatestPing(sysAddr)) << "ms";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		} else {
			std::stringstream message;
			message << "Your average ping: " << std::to_string(Game::server->GetPing(sysAddr)) << "ms";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		}
	}

	void FixStats(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		// Reset skill component and buff component
		auto* skillComponent = entity->GetComponent<SkillComponent>();
		auto* buffComponent = entity->GetComponent<BuffComponent>();
		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		// If any of the components are nullptr, return
		if (skillComponent == nullptr || buffComponent == nullptr || destroyableComponent == nullptr) {
			return;
		}

		// Reset skill component
		skillComponent->Reset();

		// Reset buff component
		buffComponent->Reset();

		// Fix the destroyable component
		destroyableComponent->FixStats();
	}

	void Credits(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto& customText = VanityUtilities::ParseMarkdown((BinaryPathFinder::GetBinaryDir() / "vanity/CREDITS.md").string());

		{
			AMFArrayValue args;

			args.Insert("state", "Story");

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", args);
		}

		entity->AddCallbackTimer(0.5f, [customText, entity]() {
			AMFArrayValue args;

			args.Insert("visible", true);
			args.Insert("text", customText);

			LOG("Sending %s", customText.c_str());

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "ToggleStoryBox", args);
			});
	}

	void Info(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto& customText = VanityUtilities::ParseMarkdown((BinaryPathFinder::GetBinaryDir() / "vanity/INFO.md").string());

		{
			AMFArrayValue args;

			args.Insert("state", "Story");

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", args);
		}

		entity->AddCallbackTimer(0.5f, [customText, entity]() {
			AMFArrayValue args;

			args.Insert("visible", true);
			args.Insert("text", customText);

			LOG("Sending %s", customText.c_str());

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "ToggleStoryBox", args);
			});
	}

	void LeaveZone(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto currentZone = Game::zoneManager->GetZone()->GetZoneID().GetMapID();
		LWOMAPID newZone = 0;

		if (currentZone == 1001 || currentZone % 100 == 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"You are not in an instanced zone.");
			return;
		} else {
			newZone = (currentZone / 100) * 100;
		}
		// If new zone would be inaccessible, then default to Avant Gardens.
		if (!Game::zoneManager->CheckIfAccessibleZone(newZone)) newZone = 1100;

		ChatPackets::SendSystemMessage(sysAddr, u"Leaving zone...");

		const auto objid = entity->GetObjectID();

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, newZone, 0, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			auto* entity = Game::entityManager->GetEntity(objid);

			if (entity == nullptr) {
				return;
			}

			const auto sysAddr = entity->GetSystemAddress();

			LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", entity->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

			if (entity->GetCharacter()) {
				entity->GetCharacter()->SetZoneID(zoneID);
				entity->GetCharacter()->SetZoneInstance(zoneInstance);
				entity->GetCharacter()->SetZoneClone(zoneClone);
			}

			entity->GetCharacter()->SaveXMLToDatabase();

			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			});
	}

	void Join(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		ChatPackets::SendSystemMessage(sysAddr, u"Requesting private map...");
		const auto& password = splitArgs[0];

		ZoneInstanceManager::Instance()->RequestPrivateZone(Game::server, false, password, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

			if (entity->GetCharacter()) {
				entity->GetCharacter()->SetZoneID(zoneID);
				entity->GetCharacter()->SetZoneInstance(zoneInstance);
				entity->GetCharacter()->SetZoneClone(zoneClone);
			}

			entity->GetCharacter()->SaveXMLToDatabase();

			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			});
	}

	void Die(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		entity->Smash(entity->GetObjectID());
	}

	void Resurrect(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		ScriptedActivityComponent* scriptedActivityComponent = Game::zoneManager->GetZoneControlObject()->GetComponent<ScriptedActivityComponent>();

		if (scriptedActivityComponent) { // check if user is in activity world and if so, they can't resurrect
			ChatPackets::SendSystemMessage(sysAddr, u"You cannot resurrect in an activity world.");
			return;
		}

		GameMessages::SendResurrect(entity);
	}

	void RequestMailCount(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		Mail::HandleNotificationRequest(entity->GetSystemAddress(), entity->GetObjectID());
	}

	void InstanceInfo(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto zoneId = Game::zoneManager->GetZone()->GetZoneID();

		ChatPackets::SendSystemMessage(sysAddr, u"Map: " + (GeneralUtils::to_u16string(zoneId.GetMapID())) + u"\nClone: " + (GeneralUtils::to_u16string(zoneId.GetCloneID())) + u"\nInstance: " + (GeneralUtils::to_u16string(zoneId.GetInstanceID())));
	}
};

namespace DEVGMCommands {
	void SetGMLevel(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		User* user = UserManager::Instance()->GetUser(entity->GetSystemAddress());

		const auto level_intermed = GeneralUtils::TryParse<uint32_t>(args);
		if (!level_intermed) {
			GameMessages::SendSlashCommandFeedbackText(entity, u"Invalid GM level.");
			return;
		}
		eGameMasterLevel level = static_cast<eGameMasterLevel>(level_intermed.value());

	#ifndef DEVELOPER_SERVER
		if (user->GetMaxGMLevel() == eGameMasterLevel::JUNIOR_DEVELOPER) {
			level = eGameMasterLevel::CIVILIAN;
		}
	#endif

		if (level > user->GetMaxGMLevel()) level = user->GetMaxGMLevel();

		if (level == entity->GetGMLevel()) return;
		bool success = user->GetMaxGMLevel() >= level;

		if (success) {
			WorldPackets::SendGMLevelChange(entity->GetSystemAddress(), success, user->GetMaxGMLevel(), entity->GetGMLevel(), level);
			GameMessages::SendChatModeUpdate(entity->GetObjectID(), level);
			entity->SetGMLevel(level);
			LOG("User %s (%i) has changed their GM level to %i for charID %llu", user->GetUsername().c_str(), user->GetAccountID(), level, entity->GetObjectID());
		}

	#ifndef DEVELOPER_SERVER
		if ((entity->GetGMLevel() > user->GetMaxGMLevel()) || (entity->GetGMLevel() > eGameMasterLevel::CIVILIAN && user->GetMaxGMLevel() == eGameMasterLevel::JUNIOR_DEVELOPER)) {
			WorldPackets::SendGMLevelChange(entity->GetSystemAddress(), true, user->GetMaxGMLevel(), entity->GetGMLevel(), eGameMasterLevel::CIVILIAN);
			GameMessages::SendChatModeUpdate(entity->GetObjectID(), eGameMasterLevel::CIVILIAN);
			entity->SetGMLevel(eGameMasterLevel::CIVILIAN);

			GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);

			GameMessages::SendSlashCommandFeedbackText(entity, u"Your game master level has been changed, you may not be able to use all commands.");
		}
	#endif
	}


	void ToggleNameplate(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if ((Game::config->GetValue("allow_nameplate_off") != "1" && entity->GetGMLevel() < eGameMasterLevel::DEVELOPER)) return;

		auto* character = entity->GetCharacter();
		if (character && character->GetBillboardVisible()) {
			character->SetBillboardVisible(false);
			GameMessages::SendSlashCommandFeedbackText(entity, u"Your nameplate has been turned off and is not visible to players currently in this zone.");
		} else {
			character->SetBillboardVisible(true);
			GameMessages::SendSlashCommandFeedbackText(entity, u"Your nameplate is now on and visible to all players.");
		}
	}

	void ToggleSkipCinematics(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (Game::config->GetValue("allow_players_to_skip_cinematics") != "1" && entity->GetGMLevel() < eGameMasterLevel::DEVELOPER) return;
		auto* character = entity->GetCharacter();
		if (!character) return;
		bool current = character->GetPlayerFlag(ePlayerFlag::DLU_SKIP_CINEMATICS);
		character->SetPlayerFlag(ePlayerFlag::DLU_SKIP_CINEMATICS, !current);
		if (!current) {
			GameMessages::SendSlashCommandFeedbackText(entity, u"You have elected to skip cinematics. Note that not all cinematics can be skipped, but most will be skipped now.");
		} else {
			GameMessages::SendSlashCommandFeedbackText(entity, u"Cinematics will no longer be skipped.");
		}
	}

	void ResetMission(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto missionId = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		if (!missionId) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission ID.");
			return;
		}

		auto* missionComponent = entity->GetComponent<MissionComponent>();
		if (!missionComponent) return;
		missionComponent->ResetMission(missionId.value());
	}

	void SetMinifig(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		const auto minifigItemIdExists = GeneralUtils::TryParse<int32_t>(splitArgs[1]);
		if (!minifigItemIdExists) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid Minifig Item Id ID.");
			return;
		}
		const int32_t minifigItemId = minifigItemIdExists.value();
		Game::entityManager->DestructEntity(entity, sysAddr);
		auto* charComp = entity->GetComponent<CharacterComponent>();
		std::string lowerName = splitArgs[0];
		if (lowerName.empty()) return;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
		if (lowerName == "eyebrows") {
			charComp->m_Character->SetEyebrows(minifigItemId);
		} else if (lowerName == "eyes") {
			charComp->m_Character->SetEyes(minifigItemId);
		} else if (lowerName == "haircolor") {
			charComp->m_Character->SetHairColor(minifigItemId);
		} else if (lowerName == "hairstyle") {
			charComp->m_Character->SetHairStyle(minifigItemId);
		} else if (lowerName == "pants") {
			charComp->m_Character->SetPantsColor(minifigItemId);
		} else if (lowerName == "lefthand") {
			charComp->m_Character->SetLeftHand(minifigItemId);
		} else if (lowerName == "mouth") {
			charComp->m_Character->SetMouth(minifigItemId);
		} else if (lowerName == "righthand") {
			charComp->m_Character->SetRightHand(minifigItemId);
		} else if (lowerName == "shirtcolor") {
			charComp->m_Character->SetShirtColor(minifigItemId);
		} else if (lowerName == "hands") {
			charComp->m_Character->SetLeftHand(minifigItemId);
			charComp->m_Character->SetRightHand(minifigItemId);
		} else {
			Game::entityManager->ConstructEntity(entity);
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid Minifig item to change, try one of the following: Eyebrows, Eyes, HairColor, HairStyle, Pants, LeftHand, Mouth, RightHand, Shirt, Hands");
			return;
		}

		Game::entityManager->ConstructEntity(entity);
		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(lowerName) + u" set to " + (GeneralUtils::to_u16string(minifigItemId)));

		GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS); // need to retoggle because it gets reenabled on creation of new character
	}

	void PlayAnimation(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		std::u16string anim = GeneralUtils::ASCIIToUTF16(splitArgs[0], splitArgs[0].size());
		RenderComponent::PlayAnimation(entity, anim);
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto* possessedComponent = Game::entityManager->GetEntity(possessorComponent->GetPossessable());
			if (possessedComponent) RenderComponent::PlayAnimation(possessedComponent, anim);
		}
	}

	void ListSpawns(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		for (const auto& pair : Game::entityManager->GetSpawnPointEntities()) {
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(pair.first));
		}

		ChatPackets::SendSystemMessage(sysAddr, u"Current: " + GeneralUtils::ASCIIToUTF16(entity->GetCharacter()->GetTargetScene()));
	}

	void UnlockEmote(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto emoteID = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

		if (!emoteID) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid emote ID.");
			return;
		}

		entity->GetCharacter()->UnlockEmote(emoteID.value());
	}

	void ForceSave(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		entity->GetCharacter()->SaveXMLToDatabase();
	}

	void Kill(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		ChatPackets::SendSystemMessage(sysAddr, u"Brutally murdering that player, if online on this server.");

		auto* player = PlayerManager::GetPlayer(splitArgs[0]);
		if (player) {
			player->Smash(entity->GetObjectID());
			ChatPackets::SendSystemMessage(sysAddr, u"It has been done, do you feel good about yourself now?");
			return;
		}

		ChatPackets::SendSystemMessage(sysAddr, u"They were saved from your carnage.");
		return;
	}

	void SpeedBoost(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto boostOptional = GeneralUtils::TryParse<float>(splitArgs[0]);
		if (!boostOptional) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid boost.");
			return;
		}
		const float boost = boostOptional.value();

		auto* controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();

		if (!controllablePhysicsComponent) return;
		controllablePhysicsComponent->SetSpeedMultiplier(boost);

		// speedboost possessables
		auto possessor = entity->GetComponent<PossessorComponent>();
		if (possessor) {
			auto possessedID = possessor->GetPossessable();
			if (possessedID != LWOOBJID_EMPTY) {
				auto possessable = Game::entityManager->GetEntity(possessedID);
				if (possessable) {
					auto* possessControllablePhysicsComponent = possessable->GetComponent<ControllablePhysicsComponent>();
					if (possessControllablePhysicsComponent) {
						possessControllablePhysicsComponent->SetSpeedMultiplier(boost);
					}
				}
			}
		}

		Game::entityManager->SerializeEntity(entity);
	}

	void Freecam(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto state = !entity->GetVar<bool>(u"freecam");
		entity->SetVar<bool>(u"freecam", state);

		GameMessages::SendSetPlayerControlScheme(entity, static_cast<eControlScheme>(state ? 9 : 1));

		ChatPackets::SendSystemMessage(sysAddr, u"Toggled freecam.");
	}

	void SetControlScheme(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto scheme = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);

		if (!scheme) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid control scheme.");
			return;
		}

		GameMessages::SendSetPlayerControlScheme(entity, static_cast<eControlScheme>(scheme.value()));

		ChatPackets::SendSystemMessage(sysAddr, u"Switched control scheme.");
	}

	void SetUiState(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		AMFArrayValue uiState;

		uiState.Insert("state", splitArgs[0]);

		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "pushGameState", uiState);

		ChatPackets::SendSystemMessage(sysAddr, u"Switched UI state.");
	}

	void Toggle(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		AMFArrayValue amfArgs;

		amfArgs.Insert("visible", true);

		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, splitArgs[0], amfArgs);

		ChatPackets::SendSystemMessage(sysAddr, u"Toggled UI state.");
	}

	void SetInventorySize(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto sizeOptional = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		if (!sizeOptional) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid size.");
			return;
		}
		const uint32_t size = sizeOptional.value();

		eInventoryType selectedInventory = eInventoryType::ITEMS;

		// a possible inventory was provided if we got more than 1 argument
		if (splitArgs.size() >= 2) {
			selectedInventory = GeneralUtils::TryParse<eInventoryType>(splitArgs.at(1)).value_or(eInventoryType::INVALID);
			if (selectedInventory == eInventoryType::INVALID) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid inventory.");
				return;
			} else {
				// In this case, we treat the input as a string and try to find it in the reflection list
				std::transform(splitArgs.at(1).begin(), splitArgs.at(1).end(), splitArgs.at(1).begin(), ::toupper);
				for (uint32_t index = 0; index < NUMBER_OF_INVENTORIES; index++) {
					if (std::string_view(splitArgs.at(1)) == std::string_view(InventoryType::InventoryTypeToString(static_cast<eInventoryType>(index)))) selectedInventory = static_cast<eInventoryType>(index);
				}
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Setting inventory " +
				GeneralUtils::ASCIIToUTF16(splitArgs.at(1)) +
				u" to size " +
				GeneralUtils::to_u16string(size));
		} else ChatPackets::SendSystemMessage(sysAddr, u"Setting inventory ITEMS to size " + GeneralUtils::to_u16string(size));

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			auto* inventory = inventoryComponent->GetInventory(selectedInventory);

			inventory->SetSize(size);
		}
	}

	void RunMacro(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		// Only process if input does not contain separator charaters
		if (splitArgs[0].find("/") != std::string::npos) return;
		if (splitArgs[0].find("\\") != std::string::npos) return;

		auto infile = Game::assetManager->GetFile(("macros/" + splitArgs[0] + ".scm").c_str());

		if (!infile) {
			ChatPackets::SendSystemMessage(sysAddr, u"Unknown macro! Is the filename right?");
			return;
		}

		if (infile.good()) {
			std::string line;
			while (std::getline(infile, line)) {
				// Do this in two separate calls to catch both \n and \r\n
				line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
				line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
				SlashCommandHandler::HandleChatCommand(GeneralUtils::ASCIIToUTF16(line), entity, sysAddr);
			}
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Unknown macro! Is the filename right?");
		}
	}

	void AddMission(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto missionID = GeneralUtils::TryParse<uint32_t>(splitArgs.at(0));

		if (!missionID) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}

		auto comp = static_cast<MissionComponent*>(entity->GetComponent(eReplicaComponentType::MISSION));
		if (comp) comp->AcceptMission(missionID.value(), true);
	}

	void CompleteMission(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto missionID = GeneralUtils::TryParse<uint32_t>(splitArgs.at(0));

		if (!missionID) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}

		auto comp = static_cast<MissionComponent*>(entity->GetComponent(eReplicaComponentType::MISSION));
		if (comp) comp->CompleteMission(missionID.value(), true);
	}

	void SetFlag(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() == 1) {
			const auto flagId = GeneralUtils::TryParse<int32_t>(splitArgs.at(0));

			if (!flagId) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
				return;
			}

			entity->GetCharacter()->SetPlayerFlag(flagId.value(), true);
		} else if (splitArgs.size() >= 2) {
			const auto flagId = GeneralUtils::TryParse<int32_t>(splitArgs.at(1));
			std::string onOffFlag = splitArgs.at(0);
			if (!flagId) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
				return;
			}

			if (onOffFlag != "off" && onOffFlag != "on") {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag type.");
				return;
			}

			entity->GetCharacter()->SetPlayerFlag(flagId.value(), onOffFlag == "on");
		}
	}

	void ClearFlag(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto flagId = GeneralUtils::TryParse<int32_t>(splitArgs.at(0));

		if (!flagId) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}

		entity->GetCharacter()->SetPlayerFlag(flagId.value(), false);
	}

	void PlayEffect(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 3) return;

		const auto effectID = GeneralUtils::TryParse<int32_t>(splitArgs.at(0));

		if (!effectID) return;

		// FIXME: use fallible ASCIIToUTF16 conversion, because non-ascii isn't valid anyway
		GameMessages::SendPlayFXEffect(entity->GetObjectID(), effectID.value(), GeneralUtils::ASCIIToUTF16(splitArgs.at(1)), splitArgs.at(2));
	}

	void StopEffect(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		GameMessages::SendStopFXEffect(entity, true, splitArgs[0]);
	}

	void SetAnnTitle(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		entity->GetCharacter()->SetAnnouncementTitle(args);
	}

	void SetAnnMsg(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		entity->GetCharacter()->SetAnnouncementMessage(args);
	}

	void Announce(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (entity->GetCharacter()->GetAnnouncementTitle().empty() || entity->GetCharacter()->GetAnnouncementMessage().empty()) {
			ChatPackets::SendSystemMessage(sysAddr, u"Use /setanntitle <title> & /setannmsg <msg> first!");
			return;
		}

		SlashCommandHandler::SendAnnouncement(entity->GetCharacter()->GetAnnouncementTitle(), entity->GetCharacter()->GetAnnouncementMessage());
	}

	void ShutdownUniverse(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		//Tell the master server that we're going to be shutting down whole "universe":
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SHUTDOWN_UNIVERSE);
		Game::server->SendToMaster(bitStream);
		ChatPackets::SendSystemMessage(sysAddr, u"Sent universe shutdown notification to master.");

		//Tell chat to send an announcement to all servers
		SlashCommandHandler::SendAnnouncement("Servers Closing Soon!", "DLU servers will close for maintenance in 10 minutes from now.");
	}

	void GetNavmeshHeight(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto control = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
		if (!control) return;

		float y = dpWorld::GetNavMesh()->GetHeightAtPoint(control->GetPosition());
		std::u16string msg = u"Navmesh height: " + (GeneralUtils::to_u16string(y));
		ChatPackets::SendSystemMessage(sysAddr, msg);
	}

	void GmAddItem(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		if (splitArgs.size() == 1) {
			const auto itemLOT = GeneralUtils::TryParse<uint32_t>(splitArgs.at(0));

			if (!itemLOT) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}

			InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));

			inventory->AddItem(itemLOT.value(), 1, eLootSourceType::MODERATION);
		} else if (splitArgs.size() == 2) {
			const auto itemLOT = GeneralUtils::TryParse<uint32_t>(splitArgs.at(0));
			if (!itemLOT) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}

			const auto count = GeneralUtils::TryParse<uint32_t>(splitArgs.at(1));
			if (!count) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item count.");
				return;
			}

			InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));

			inventory->AddItem(itemLOT.value(), count.value(), eLootSourceType::MODERATION);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /gmadditem <lot>");
		}
	}

	void Teleport(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		NiPoint3 pos{};
		if (splitArgs.size() == 3) {

			const auto x = GeneralUtils::TryParse<float>(splitArgs.at(0));
			if (!x) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}

			const auto y = GeneralUtils::TryParse<float>(splitArgs.at(1));
			if (!y) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid y.");
				return;
			}

			const auto z = GeneralUtils::TryParse<float>(splitArgs.at(2));
			if (!z) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

			pos.SetX(x.value());
			pos.SetY(y.value());
			pos.SetZ(z.value());

			LOG("Teleporting objectID: %llu to %f, %f, %f", entity->GetObjectID(), pos.x, pos.y, pos.z);
			GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
		} else if (splitArgs.size() == 2) {

			const auto x = GeneralUtils::TryParse<float>(splitArgs.at(0));
			if (!x) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}

			const auto z = GeneralUtils::TryParse<float>(splitArgs.at(1));
			if (!z) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

			pos.SetX(x.value());
			pos.SetY(0.0f);
			pos.SetZ(z.value());

			LOG("Teleporting objectID: %llu to X: %f, Z: %f", entity->GetObjectID(), pos.x, pos.z);
			GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /teleport <x> (<y>) <z> - if no Y given, will teleport to the height of the terrain (or any physics object).");
		}

		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto* possassableEntity = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

			if (possassableEntity != nullptr) {
				auto* havokVehiclePhysicsComponent = possassableEntity->GetComponent<HavokVehiclePhysicsComponent>();
				if (havokVehiclePhysicsComponent) {
					havokVehiclePhysicsComponent->SetPosition(pos);
					Game::entityManager->SerializeEntity(possassableEntity);
				} else GameMessages::SendTeleport(possassableEntity->GetObjectID(), pos, NiQuaternion(), sysAddr);
			}
		}
	}

	void TpAll(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto pos = entity->GetPosition();

		const auto characters = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::CHARACTER);

		for (auto* character : characters) {
			GameMessages::SendTeleport(character->GetObjectID(), pos, NiQuaternion(), character->GetSystemAddress());
		}
	}

	void Dismount(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto possessableId = possessorComponent->GetPossessable();
			if (possessableId != LWOOBJID_EMPTY) {
				auto* possessableEntity = Game::entityManager->GetEntity(possessableId);
				if (possessableEntity) possessorComponent->Dismount(possessableEntity, true);
			}
		}
	}

	void BuffMe(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(999);
			dest->SetMaxHealth(999.0f);
			dest->SetArmor(999);
			dest->SetMaxArmor(999.0f);
			dest->SetImagination(999);
			dest->SetMaxImagination(999.0f);
		}
		Game::entityManager->SerializeEntity(entity);
	}

	void StartCelebration(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto celebration = GeneralUtils::TryParse<int32_t>(splitArgs.at(0));

		if (!celebration) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid celebration.");
			return;
		}

		GameMessages::SendStartCelebrationEffect(entity, entity->GetSystemAddress(), celebration.value());
	}

	void BuffMed(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(9);
			dest->SetMaxHealth(9.0f);
			dest->SetArmor(9);
			dest->SetMaxArmor(9.0f);
			dest->SetImagination(9);
			dest->SetMaxImagination(9.0f);
		}
		Game::entityManager->SerializeEntity(entity);
	}

	void RefillStats(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(static_cast<int32_t>(dest->GetMaxHealth()));
			dest->SetArmor(static_cast<int32_t>(dest->GetMaxArmor()));
			dest->SetImagination(static_cast<int32_t>(dest->GetMaxImagination()));
		}

		Game::entityManager->SerializeEntity(entity);
	}

	void Lookup(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto query = CDClientDatabase::CreatePreppedStmt(
			"SELECT `id`, `name` FROM `Objects` WHERE `displayName` LIKE ?1 OR `name` LIKE ?1 OR `description` LIKE ?1 LIMIT 50");

		const std::string query_text = "%" + args + "%";
		query.bind(1, query_text.c_str());

		auto tables = query.execQuery();

		while (!tables.eof()) {
			std::string message = std::to_string(tables.getIntField(0)) + " - " + tables.getStringField(1);
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(message, message.size()));
			tables.nextRow();
		}
	}

	void Spawn(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		ControllablePhysicsComponent* comp = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
		if (!comp) return;

		const auto lot = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);

		if (!lot) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid lot.");
			return;
		}

		EntityInfo info;
		info.lot = lot.value();
		info.pos = comp->GetPosition();
		info.rot = comp->GetRotation();
		info.spawner = nullptr;
		info.spawnerID = entity->GetObjectID();
		info.spawnerNodeID = 0;

		Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);

		if (newEntity == nullptr) {
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to spawn entity.");
			return;
		}

		Game::entityManager->ConstructEntity(newEntity);
	}

	void SpawnGroup(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 3) return;

		const auto lot = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		if (!lot) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid lot.");
			return;
		}

		const auto numberToSpawnOptional = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
		if (!numberToSpawnOptional && numberToSpawnOptional.value() > 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid number of enemies to spawn.");
			return;
		}
		uint32_t numberToSpawn = numberToSpawnOptional.value();

		// Must spawn within a radius of at least 0.0f
		const auto radiusToSpawnWithinOptional = GeneralUtils::TryParse<float>(splitArgs[2]);
		if (!radiusToSpawnWithinOptional && radiusToSpawnWithinOptional.value() < 0.0f) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid radius to spawn within.");
			return;
		}
		const float radiusToSpawnWithin = radiusToSpawnWithinOptional.value();

		EntityInfo info;
		info.lot = lot.value();
		info.spawner = nullptr;
		info.spawnerID = entity->GetObjectID();
		info.spawnerNodeID = 0;

		auto playerPosition = entity->GetPosition();
		while (numberToSpawn > 0) {
			auto randomAngle = GeneralUtils::GenerateRandomNumber<float>(0.0f, 2 * PI);
			auto randomRadius = GeneralUtils::GenerateRandomNumber<float>(0.0f, radiusToSpawnWithin);

			// Set the position to the generated random position plus the player position.  This will
			// spawn the entity in a circle around the player.  As you get further from the player, the angle chosen will get less accurate.
			info.pos = playerPosition + NiPoint3(cos(randomAngle) * randomRadius, 0.0f, sin(randomAngle) * randomRadius);
			info.rot = NiQuaternion();

			auto newEntity = Game::entityManager->CreateEntity(info);
			if (newEntity == nullptr) {
			}
		}
	}

	void GiveUScore(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto uscoreOptional = GeneralUtils::TryParse<int32_t>(splitArgs[0]);
		if (!uscoreOptional) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid uscore.");
			return;
		}
		const int32_t uscore = uscoreOptional.value();

		CharacterComponent* character = entity->GetComponent<CharacterComponent>();
		if (character) character->SetUScore(character->GetUScore() + uscore);
		// MODERATION should work but it doesn't.  Relog to see uscore changes

		eLootSourceType lootType = eLootSourceType::MODERATION;

		if (splitArgs.size() >= 2) {
			const auto type = GeneralUtils::TryParse<eLootSourceType>(splitArgs[1]);
			lootType = type.value_or(lootType);
		}

		GameMessages::SendModifyLEGOScore(entity, entity->GetSystemAddress(), uscore, lootType);
	}

	void SetLevel(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		// We may be trying to set a specific players level to a level.  If so override the entity with the requested players.
		std::string requestedPlayerToSetLevelOf = "";
		if (splitArgs.size() > 1) {
			requestedPlayerToSetLevelOf = splitArgs[1];

			auto requestedPlayer = PlayerManager::GetPlayer(requestedPlayerToSetLevelOf);

			if (!requestedPlayer) {
				ChatPackets::SendSystemMessage(sysAddr, u"No player found with username: (" + GeneralUtils::UTF8ToUTF16(requestedPlayerToSetLevelOf) + u").");
				return;
			}

			if (!requestedPlayer->GetOwner()) {
				ChatPackets::SendSystemMessage(sysAddr, u"No entity found with username: (" + GeneralUtils::UTF8ToUTF16(requestedPlayerToSetLevelOf) + u").");
				return;
			}

			entity = requestedPlayer->GetOwner();
		}
		const auto requestedLevelOptional = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		uint32_t oldLevel;

		// first check the level is valid
		if (!requestedLevelOptional) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid level.");
			return;
		}
		uint32_t requestedLevel = requestedLevelOptional.value();
		// query to set our uscore to the correct value for this level

		auto characterComponent = entity->GetComponent<CharacterComponent>();
		if (!characterComponent) return;
		auto levelComponent = entity->GetComponent<LevelProgressionComponent>();
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT requiredUScore from LevelProgressionLookup WHERE id = ?;");
		query.bind(1, static_cast<int>(requestedLevel));
		auto result = query.execQuery();

		if (result.eof()) return;

		// Set the UScore first
		oldLevel = levelComponent->GetLevel();
		characterComponent->SetUScore(result.getIntField(0, characterComponent->GetUScore()));

		// handle level up for each level we have passed if we set our level to be higher than the current one.
		if (oldLevel < requestedLevel) {
			while (oldLevel < requestedLevel) {
				oldLevel += 1;
				levelComponent->SetLevel(oldLevel);
				levelComponent->HandleLevelUp();
			}
		} else {
			levelComponent->SetLevel(requestedLevel);
		}

		if (requestedPlayerToSetLevelOf != "") {
			ChatPackets::SendSystemMessage(
				sysAddr, u"Set " + GeneralUtils::UTF8ToUTF16(requestedPlayerToSetLevelOf) + u"'s level to " + GeneralUtils::to_u16string(requestedLevel) +
				u" and UScore to " + GeneralUtils::to_u16string(characterComponent->GetUScore()) +
				u". Relog to see changes.");
		} else {
			ChatPackets::SendSystemMessage(
				sysAddr, u"Set your level to " + GeneralUtils::to_u16string(requestedLevel) +
				u" and UScore to " + GeneralUtils::to_u16string(characterComponent->GetUScore()) +
				u". Relog to see changes.");
		}
	}

	void Pos(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto position = entity->GetPosition();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(position.x)) + u", " + (GeneralUtils::to_u16string(position.y)) + u", " + (GeneralUtils::to_u16string(position.z)) + u">");

		LOG("Position: %f, %f, %f", position.x, position.y, position.z);
	}

	void Rot(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto rotation = entity->GetRotation();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(rotation.w)) + u", " + (GeneralUtils::to_u16string(rotation.x)) + u", " + (GeneralUtils::to_u16string(rotation.y)) + u", " + (GeneralUtils::to_u16string(rotation.z)) + u">");

		LOG("Rotation: %f, %f, %f, %f", rotation.w, rotation.x, rotation.y, rotation.z);
	}

	void LocRow(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto position = entity->GetPosition();
		const auto rotation = entity->GetRotation();

		LOG("<location x=\"%f\" y=\"%f\" z=\"%f\" rw=\"%f\" rx=\"%f\" ry=\"%f\" rz=\"%f\" />", position.x, position.y, position.z, rotation.w, rotation.x, rotation.y, rotation.z);
	}

	void PlayLvlFx(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendPlayFXEffect(entity, 7074, u"create", "7074", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}

	void PlayRebuildFx(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendPlayFXEffect(entity, 230, u"rebuild", "230", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}

	void FreeMoney(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto money = GeneralUtils::TryParse<int64_t>(splitArgs[0]);

		if (!money) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(ch->GetCoins() + money.value(), eLootSourceType::MODERATION);
	}

	void SetCurrency(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		const auto money = GeneralUtils::TryParse<int64_t>(splitArgs[0]);

		if (!money) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(money.value(), eLootSourceType::MODERATION);
	}

	void Buff(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		auto* buffComponent = entity->GetComponent<BuffComponent>();

		const auto id = GeneralUtils::TryParse<int32_t>(splitArgs[0]);
		if (!id) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff id.");
			return;
		}

		const auto duration = GeneralUtils::TryParse<int32_t>(splitArgs[1]);
		if (!duration) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff duration.");
			return;
		}

		if (buffComponent) buffComponent->ApplyBuff(id.value(), duration.value(), entity->GetObjectID());
	}

	void TestMap(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		ChatPackets::SendSystemMessage(sysAddr, u"Requesting map change...");
		LWOCLONEID cloneId = 0;
		bool force = false;

		const auto reqZoneOptional = GeneralUtils::TryParse<LWOMAPID>(splitArgs[0]);
		if (!reqZoneOptional) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}
		const LWOMAPID reqZone = reqZoneOptional.value();

		if (splitArgs.size() > 1) {
			auto index = 1;

			if (splitArgs[index] == "force") {
				index++;

				force = true;
			}

			if (splitArgs.size() > index) {
				const auto cloneIdOptional = GeneralUtils::TryParse<LWOCLONEID>(splitArgs[index]);
				if (!cloneIdOptional) {
					ChatPackets::SendSystemMessage(sysAddr, u"Invalid clone id.");
					return;
				}
				cloneId = cloneIdOptional.value();
			}
		}

		const auto objid = entity->GetObjectID();

		if (force || Game::zoneManager->CheckIfAccessibleZone(reqZone)) { // to prevent tomfoolery

			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, reqZone, cloneId, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {

				auto* entity = Game::entityManager->GetEntity(objid);
				if (!entity) return;

				const auto sysAddr = entity->GetSystemAddress();

				ChatPackets::SendSystemMessage(sysAddr, u"Transfering map...");

				LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
				if (entity->GetCharacter()) {
					entity->GetCharacter()->SetZoneID(zoneID);
					entity->GetCharacter()->SetZoneInstance(zoneInstance);
					entity->GetCharacter()->SetZoneClone(zoneClone);
					entity->GetComponent<CharacterComponent>()->SetLastRocketConfig(u"");
				}

				entity->GetCharacter()->SaveXMLToDatabase();

				WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
				return;
				});
		} else {
			std::string msg = "ZoneID not found or allowed: ";
			msg.append(splitArgs[0]); // FIXME: unnecessary utf16 re-encoding just for error
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(msg, msg.size()));
		}
	}

	void CreatePrivate(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 3) return;

		const auto zone = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		if (!zone) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}

		const auto clone = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
		if (!clone) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid clone.");
			return;
		}

		const auto& password = splitArgs[2];

		ZoneInstanceManager::Instance()->CreatePrivateZone(Game::server, zone.value(), clone.value(), password);

		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16("Sent request for private zone with password: " + password));
	}

	void DebugUi(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		ChatPackets::SendSystemMessage(sysAddr, u"Opening UIDebugger...");
	}

	void Boost(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		auto* vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		if (splitArgs.size() >= 1) {
			const auto time = GeneralUtils::TryParse<float>(splitArgs[0]);

			if (!time) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid boost time.");
				return;
			} else {
				GameMessages::SendVehicleAddPassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
				entity->AddCallbackTimer(time.value(), [vehicle]() {
					if (!vehicle) return;
					GameMessages::SendVehicleRemovePassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
					});
			}
		} else {
			GameMessages::SendVehicleAddPassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		}
	}

	void Unboost(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) return;
		auto* vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) return;
		GameMessages::SendVehicleRemovePassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void ActivateSpawner(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto spawners = Game::zoneManager->GetSpawnersByName(splitArgs[0]);

		for (auto* spawner : spawners) {
			spawner->Activate();
		}

		spawners = Game::zoneManager->GetSpawnersInGroup(splitArgs[0]);

		for (auto* spawner : spawners) {
			spawner->Activate();
		}
	}

	void SpawnPhysicsVerts(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		//Go tell physics to spawn all the vertices:
		auto entities = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::PHANTOM_PHYSICS);
		for (auto en : entities) {
			auto phys = static_cast<PhantomPhysicsComponent*>(en->GetComponent(eReplicaComponentType::PHANTOM_PHYSICS));
			if (phys)
				phys->SpawnVertices();
		}
	}

	void ReportProxPhys(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto entities = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::PROXIMITY_MONITOR);
		for (auto en : entities) {
			auto phys = static_cast<ProximityMonitorComponent*>(en->GetComponent(eReplicaComponentType::PROXIMITY_MONITOR));
			if (phys) {
				for (auto prox : phys->GetProximitiesData()) {
					if (!prox.second) continue;

					auto sphere = static_cast<dpShapeSphere*>(prox.second->GetShape());
					auto pos = prox.second->GetPosition();
					LOG("Proximity: %s, r: %f, pos: %f, %f, %f", prox.first.c_str(), sphere->GetRadius(), pos.x, pos.y, pos.z);
				}
			}
		}
	}

	void TriggerSpawner(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto spawners = Game::zoneManager->GetSpawnersByName(splitArgs[0]);

		for (auto* spawner : spawners) {
			spawner->Spawn();
		}

		spawners = Game::zoneManager->GetSpawnersInGroup(splitArgs[0]);

		for (auto* spawner : spawners) {
			spawner->Spawn();
		}
	}

	void Reforge(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		const auto baseItem = GeneralUtils::TryParse<LOT>(splitArgs[0]);
		if (!baseItem) return;

		const auto reforgedItem = GeneralUtils::TryParse<LOT>(splitArgs[1]);
		if (!reforgedItem) return;

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;

		std::vector<LDFBaseData*> data{};
		data.push_back(new LDFData<int32_t>(u"reforgedLOT", reforgedItem.value()));

		inventoryComponent->AddItem(baseItem.value(), 1, eLootSourceType::MODERATION, eInventoryType::INVALID, data);
	}

	void Crash(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		ChatPackets::SendSystemMessage(sysAddr, u"Crashing...");

		int* badPtr = nullptr;
		*badPtr = 0;
	}

	void Metrics(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		for (const auto variable : Metrics::GetAllMetrics()) {
			auto* metric = Metrics::GetMetric(variable);

			if (metric == nullptr) {
				continue;
			}

			ChatPackets::SendSystemMessage(
				sysAddr,
				GeneralUtils::ASCIIToUTF16(Metrics::MetricVariableToString(variable)) +
				u": " +
				GeneralUtils::to_u16string(Metrics::ToMiliseconds(metric->average)) +
				u"ms"
			);
		}

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Peak RSS: " + GeneralUtils::to_u16string(static_cast<float>(static_cast<double>(Metrics::GetPeakRSS()) / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Current RSS: " + GeneralUtils::to_u16string(static_cast<float>(static_cast<double>(Metrics::GetCurrentRSS()) / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Process ID: " + GeneralUtils::to_u16string(Metrics::GetProcessID())
		);
	}

	void ReloadConfig(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		Game::config->ReloadConfig();
		VanityUtilities::SpawnVanity();
		dpWorld::Reload();
		auto entities = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
		for (const auto* const entity : entities) {
			auto* const scriptedActivityComponent = entity->GetComponent<ScriptedActivityComponent>();
			if (!scriptedActivityComponent) continue;

			scriptedActivityComponent->ReloadConfig();
		}
		Game::server->UpdateMaximumMtuSize();
		Game::server->UpdateBandwidthLimit();
		ChatPackets::SendSystemMessage(sysAddr, u"Successfully reloaded config for world!");
	}

	void RollLoot(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 3) return;

		const auto lootMatrixIndex = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);
		if (!lootMatrixIndex) return;

		const auto targetLot = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
		if (!targetLot) return;

		const auto loops = GeneralUtils::TryParse<uint32_t>(splitArgs[2]);
		if (!loops) return;

		uint64_t totalRuns = 0;

		for (uint32_t i = 0; i < loops; i++) {
			while (true) {
				auto lootRoll = Loot::RollLootMatrix(lootMatrixIndex.value());
				totalRuns += 1;
				bool doBreak = false;
				for (const auto& kv : lootRoll) {
					if (static_cast<uint32_t>(kv.first) == targetLot) {
						doBreak = true;
					}
				}
				if (doBreak) break;
			}
		}

		std::u16string message = u"Ran loot drops looking for "
			+ GeneralUtils::to_u16string(targetLot.value())
			+ u", "
			+ GeneralUtils::to_u16string(loops.value())
			+ u" times. It ran "
			+ GeneralUtils::to_u16string(totalRuns)
			+ u" times. Averaging out at "
			+ GeneralUtils::to_u16string(static_cast<float>(totalRuns) / loops.value());

		ChatPackets::SendSystemMessage(sysAddr, message);
	}

	void DeleteInven(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		eInventoryType inventoryType = eInventoryType::INVALID;

		const auto inventoryTypeOptional = GeneralUtils::TryParse<eInventoryType>(splitArgs[0]);
		if (!inventoryTypeOptional) {
			// In this case, we treat the input as a string and try to find it in the reflection list
			std::transform(splitArgs[0].begin(), splitArgs[0].end(), splitArgs[0].begin(), ::toupper);
			LOG("looking for inventory %s", splitArgs[0].c_str());
			for (uint32_t index = 0; index < NUMBER_OF_INVENTORIES; index++) {
				if (std::string_view(splitArgs[0]) == std::string_view(InventoryType::InventoryTypeToString(static_cast<eInventoryType>(index)))) inventoryType = static_cast<eInventoryType>(index);
			}
		} else {
			inventoryType = inventoryTypeOptional.value();
		}

		if (inventoryType == eInventoryType::INVALID || inventoryType >= NUMBER_OF_INVENTORIES) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid inventory provided.");
			return;
		}

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;

		auto* inventoryToDelete = inventoryComponent->GetInventory(inventoryType);
		if (!inventoryToDelete) return;

		inventoryToDelete->DeleteAllItems();
		LOG("Deleted inventory %s for user %llu", splitArgs[0].c_str(), entity->GetObjectID());
		ChatPackets::SendSystemMessage(sysAddr, u"Deleted inventory " + GeneralUtils::UTF8ToUTF16(splitArgs[0]));
	}

	void CastSkill(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* skillComponent = entity->GetComponent<SkillComponent>();
		if (skillComponent) {
			const auto skillId = GeneralUtils::TryParse<uint32_t>(splitArgs[0]);

			if (!skillId) {
				ChatPackets::SendSystemMessage(sysAddr, u"Error getting skill ID.");
				return;
			} else {
				skillComponent->CastSkill(skillId.value(), entity->GetObjectID(), entity->GetObjectID());
				ChatPackets::SendSystemMessage(sysAddr, u"Cast skill");
			}
		}
	}

	void SetSkillSlot(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		auto* const inventoryComponent = entity->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			const auto slot = GeneralUtils::TryParse<BehaviorSlot>(splitArgs[0]);
			if (!slot) {
				ChatPackets::SendSystemMessage(sysAddr, u"Error getting slot.");
				return;
			} else {
				const auto skillId = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
				if (!skillId) {
					ChatPackets::SendSystemMessage(sysAddr, u"Error getting skill.");
					return;
				} else {
					if (inventoryComponent->SetSkill(slot.value(), skillId.value())) ChatPackets::SendSystemMessage(sysAddr, u"Set skill to slot successfully");
					else ChatPackets::SendSystemMessage(sysAddr, u"Set skill to slot failed");
				}
			}
		}
	}

	void SetFaction(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
		if (destroyableComponent) {
			const auto faction = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

			if (!faction) {
				ChatPackets::SendSystemMessage(sysAddr, u"Error getting faction.");
				return;
			} else {
				destroyableComponent->SetFaction(faction.value());
				ChatPackets::SendSystemMessage(sysAddr, u"Set faction and updated enemies list");
			}
		}
	}

	void AddFaction(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
		if (destroyableComponent) {
			const auto faction = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

			if (!faction) {
				ChatPackets::SendSystemMessage(sysAddr, u"Error getting faction.");
				return;
			} else {
				destroyableComponent->AddFaction(faction.value());
				ChatPackets::SendSystemMessage(sysAddr, u"Added faction and updated enemies list");
			}
		}
	}

	void GetFactions(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
		if (destroyableComponent) {
			ChatPackets::SendSystemMessage(sysAddr, u"Friendly factions:");
			for (const auto entry : destroyableComponent->GetFactionIDs()) {
				ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Enemy factions:");
			for (const auto entry : destroyableComponent->GetEnemyFactionsIDs()) {
				ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
			}
		}
	}

	void SetRewardCode(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* character = entity->GetCharacter();
		if (!character) return;
		auto* user = character->GetParentUser();
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;
		auto* cdrewardCodes = CDClientManager::GetTable<CDRewardCodesTable>();

		auto id = cdrewardCodes->GetCodeID(splitArgs[0]);
		if (id != -1) Database::Get()->InsertRewardCode(user->GetAccountID(), id);
	}

	void Inspect(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		Entity* closest = nullptr;

		std::u16string ldf;

		bool isLDF = false;

		auto component = GeneralUtils::TryParse<eReplicaComponentType>(splitArgs[0]);
		if (!component) {
			component = eReplicaComponentType::INVALID;

			ldf = GeneralUtils::UTF8ToUTF16(splitArgs[0]);

			isLDF = true;
		}

		auto reference = entity->GetPosition();

		auto closestDistance = 0.0f;

		const auto candidates = Game::entityManager->GetEntitiesByComponent(component.value());

		for (auto* candidate : candidates) {
			if (candidate->GetLOT() == 1 || candidate->GetLOT() == 8092) {
				continue;
			}

			if (isLDF && !candidate->HasVar(ldf)) {
				continue;
			}

			if (!closest) {
				closest = candidate;

				closestDistance = NiPoint3::Distance(candidate->GetPosition(), reference);

				continue;
			}

			const auto distance = NiPoint3::Distance(candidate->GetPosition(), reference);

			if (distance < closestDistance) {
				closest = candidate;

				closestDistance = distance;
			}
		}

		if (!closest) return;

		Game::entityManager->SerializeEntity(closest);

		auto* table = CDClientManager::GetTable<CDObjectsTable>();

		const auto& info = table->GetByID(closest->GetLOT());

		std::stringstream header;

		header << info.name << " [" << std::to_string(info.id) << "]" << " " << std::to_string(closestDistance) << " " << std::to_string(closest->IsSleeping());

		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(header.str()));

		for (const auto& pair : closest->GetComponents()) {
			auto id = pair.first;

			std::stringstream stream;

			stream << "Component [" << std::to_string(static_cast<uint32_t>(id)) << "]";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(stream.str()));
		}

		if (splitArgs.size() >= 2) {
			if (splitArgs[1] == "-m" && splitArgs.size() >= 3) {
				auto* const movingPlatformComponent = closest->GetComponent<MovingPlatformComponent>();

				const auto mValue = GeneralUtils::TryParse<int32_t>(splitArgs[2]);

				if (!movingPlatformComponent || !mValue) return;

				movingPlatformComponent->SetSerialized(true);

				if (mValue == -1) {
					movingPlatformComponent->StopPathing();
				} else {
					movingPlatformComponent->GotoWaypoint(mValue.value());
				}

				Game::entityManager->SerializeEntity(closest);
			} else if (splitArgs[1] == "-a" && splitArgs.size() >= 3) {
				RenderComponent::PlayAnimation(closest, splitArgs.at(2));
			} else if (splitArgs[1] == "-s") {
				for (auto* entry : closest->GetSettings()) {
					ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(entry->GetString()));
				}

				ChatPackets::SendSystemMessage(sysAddr, u"------");
				ChatPackets::SendSystemMessage(sysAddr, u"Spawner ID: " + GeneralUtils::to_u16string(closest->GetSpawnerID()));
			} else if (splitArgs[1] == "-p") {
				const auto postion = closest->GetPosition();

				ChatPackets::SendSystemMessage(
					sysAddr,
					GeneralUtils::ASCIIToUTF16("< " + std::to_string(postion.x) + ", " + std::to_string(postion.y) + ", " + std::to_string(postion.z) + " >")
				);
			} else if (splitArgs[1] == "-f") {
				auto* destuctable = closest->GetComponent<DestroyableComponent>();

				if (destuctable == nullptr) {
					ChatPackets::SendSystemMessage(sysAddr, u"No destroyable component on this entity!");
					return;
				}

				ChatPackets::SendSystemMessage(sysAddr, u"Smashable: " + (GeneralUtils::to_u16string(destuctable->GetIsSmashable())));

				ChatPackets::SendSystemMessage(sysAddr, u"Friendly factions:");
				for (const auto entry : destuctable->GetFactionIDs()) {
					ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
				}

				ChatPackets::SendSystemMessage(sysAddr, u"Enemy factions:");
				for (const auto entry : destuctable->GetEnemyFactionsIDs()) {
					ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
				}

				if (splitArgs.size() >= 3) {
					const auto faction = GeneralUtils::TryParse<int32_t>(splitArgs[2]);
					if (!faction) return;

					destuctable->SetFaction(-1);
					destuctable->AddFaction(faction.value(), true);
				}
			} else if (splitArgs[1] == "-cf") {
				auto* destuctable = entity->GetComponent<DestroyableComponent>();
				if (!destuctable) {
					ChatPackets::SendSystemMessage(sysAddr, u"No destroyable component on this entity!");
					return;
				}
				if (destuctable->IsEnemy(closest)) ChatPackets::SendSystemMessage(sysAddr, u"They are our enemy");
				else ChatPackets::SendSystemMessage(sysAddr, u"They are NOT our enemy");
			} else if (splitArgs[1] == "-t") {
				auto* phantomPhysicsComponent = closest->GetComponent<PhantomPhysicsComponent>();

				if (phantomPhysicsComponent != nullptr) {
					ChatPackets::SendSystemMessage(sysAddr, u"Type: " + (GeneralUtils::to_u16string(static_cast<uint32_t>(phantomPhysicsComponent->GetEffectType()))));
					const auto dir = phantomPhysicsComponent->GetDirection();
					ChatPackets::SendSystemMessage(sysAddr, u"Direction: <" + (GeneralUtils::to_u16string(dir.x)) + u", " + (GeneralUtils::to_u16string(dir.y)) + u", " + (GeneralUtils::to_u16string(dir.z)) + u">");
					ChatPackets::SendSystemMessage(sysAddr, u"Multiplier: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetDirectionalMultiplier())));
					ChatPackets::SendSystemMessage(sysAddr, u"Active: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetPhysicsEffectActive())));
				}

				auto* triggerComponent = closest->GetComponent<TriggerComponent>();
				if (triggerComponent) {
					auto trigger = triggerComponent->GetTrigger();
					if (trigger) {
						ChatPackets::SendSystemMessage(sysAddr, u"Trigger: " + (GeneralUtils::to_u16string(trigger->id)));
					}
				}
			}
		}
	}
};

namespace GMGreaterThanZeroCommands {

	void Kick(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() == 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			std::u16string username = GeneralUtils::UTF8ToUTF16(splitArgs[0]);
			if (player == nullptr) {
				ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + username);
				return;
			}

			Game::server->Disconnect(player->GetSystemAddress(), eServerDisconnectIdentifiers::KICK);

			ChatPackets::SendSystemMessage(sysAddr, u"Kicked: " + username);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /kick <username>");
		}
	}

	void Ban(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		if (splitArgs.size() == 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			uint32_t accountId = 0;

			if (player == nullptr) {
				auto characterInfo = Database::Get()->GetCharacterInfo(splitArgs[0]);

				if (characterInfo) {
					accountId = characterInfo->accountId;
				}

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]));

					return;
				}
			} else {
				auto* character = player->GetCharacter();
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) accountId = user->GetAccountID();
			}

			if (accountId != 0) Database::Get()->UpdateAccountBan(accountId, true);

			if (player != nullptr) {
				Game::server->Disconnect(player->GetSystemAddress(), eServerDisconnectIdentifiers::FREE_TRIAL_EXPIRED);
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Banned: " + GeneralUtils::ASCIIToUTF16(splitArgs[0]));
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /ban <username>");
		}
	}

	void MailItem(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.size() < 2) return;

		const auto& playerName = splitArgs[0];

		auto playerInfo = Database::Get()->GetCharacterInfo(playerName);

		uint32_t receiverID = 0;
		if (!playerInfo) {
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to find that player");

			return;
		}

		receiverID = playerInfo->id;

		const auto lot = GeneralUtils::TryParse<LOT>(splitArgs.at(1));

		if (!lot) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid item lot.");
			return;
		}

		IMail::MailInfo mailInsert;
		mailInsert.senderId = entity->GetObjectID();
		mailInsert.senderUsername = "Darkflame Universe";
		mailInsert.receiverId = receiverID;
		mailInsert.recipient = playerName;
		mailInsert.subject = "Lost item";
		mailInsert.body = "This is a replacement item for one you lost.";
		mailInsert.itemID = LWOOBJID_EMPTY;
		mailInsert.itemLOT = lot.value();
		mailInsert.itemSubkey = LWOOBJID_EMPTY;
		mailInsert.itemCount = 1;
		Database::Get()->InsertNewMail(mailInsert);

		ChatPackets::SendSystemMessage(sysAddr, u"Mail sent");
	}

	void ApproveProperty(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		if (PropertyManagementComponent::Instance() != nullptr) {
			PropertyManagementComponent::Instance()->UpdateApprovedStatus(true);
		}
	}

	void Mute(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');

		if (splitArgs.size() >= 1) {
			auto* player = PlayerManager::GetPlayer(splitArgs[0]);

			uint32_t accountId = 0;
			LWOOBJID characterId = 0;

			if (player == nullptr) {
				auto characterInfo = Database::Get()->GetCharacterInfo(splitArgs[0]);

				if (characterInfo) {
					accountId = characterInfo->accountId;
					characterId = characterInfo->id;

					GeneralUtils::SetBit(characterId, eObjectBits::CHARACTER);
					GeneralUtils::SetBit(characterId, eObjectBits::PERSISTENT);
				}

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]));

					return;
				}
			} else {
				auto* character = player->GetCharacter();
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) accountId = user->GetAccountID();
				characterId = player->GetObjectID();
			}

			time_t expire = 1; // Default to indefinate mute

			if (splitArgs.size() >= 2) {
				const auto days = GeneralUtils::TryParse<uint32_t>(splitArgs[1]);
				if (!days) {
					ChatPackets::SendSystemMessage(sysAddr, u"Invalid days.");

					return;
				}

				std::optional<uint32_t> hours;
				if (splitArgs.size() >= 3) {
					hours = GeneralUtils::TryParse<uint32_t>(splitArgs[2]);
					if (!hours) {
						ChatPackets::SendSystemMessage(sysAddr, u"Invalid hours.");

						return;
					}
				}

				expire = time(NULL);
				expire += 24 * 60 * 60 * days.value();
				expire += 60 * 60 * hours.value_or(0);
			}

			if (accountId != 0) Database::Get()->UpdateAccountUnmuteTime(accountId, expire);

			char buffer[32] = "brought up for review.\0";

			if (expire != 1) {
				std::tm* ptm = std::localtime(&expire);
				// Format: Mo, 15.06.2009 20:20:00
				std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
			}

			const auto timeStr = GeneralUtils::ASCIIToUTF16(std::string(buffer));

			ChatPackets::SendSystemMessage(sysAddr, u"Muted: " + GeneralUtils::UTF8ToUTF16(splitArgs[0]) + u" until " + timeStr);

			//Notify chat about it
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_MUTE);

			bitStream.Write(characterId);
			bitStream.Write(expire);

			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /mute <username> <days (optional)> <hours (optional)>");
		}
	}

	void Fly(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		auto* character = entity->GetCharacter();

		if (character) {
			bool isFlying = character->GetIsFlying();

			if (isFlying) {
				GameMessages::SendSetJetPackMode(entity, false);

				character->SetIsFlying(false);
			} else {
				float speedScale = 1.0f;

				if (splitArgs.size() >= 1) {
					const auto tempScaleStore = GeneralUtils::TryParse<float>(splitArgs.at(0));

					if (tempScaleStore) {
						speedScale = tempScaleStore.value();
					} else {
						ChatPackets::SendSystemMessage(sysAddr, u"Failed to parse speed scale argument.");
					}
				}

				float airSpeed = 20 * speedScale;
				float maxAirSpeed = 30 * speedScale;
				float verticalVelocity = 1.5 * speedScale;

				GameMessages::SendSetJetPackMode(entity, true, true, false, 167, airSpeed, maxAirSpeed, verticalVelocity);

				character->SetIsFlying(true);
			}
		}
	}

	void AttackImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		const auto state = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

		if (!state) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent) destroyableComponent->SetIsImmune(state.value());
	}

	void GmImmune(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		const auto splitArgs = GeneralUtils::SplitString(args, ' ');
		if (splitArgs.empty()) return;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		const auto state = GeneralUtils::TryParse<int32_t>(splitArgs[0]);

		if (!state) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent) destroyableComponent->SetIsGMImmune(state.value());
	}

	void GmInvis(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
	}

	void SetName(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(args), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void Title(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		std::string name = entity->GetCharacter()->GetName() + " - " + args;
		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
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
