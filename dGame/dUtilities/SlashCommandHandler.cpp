/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "SlashCommandHandler.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <exception>
#include "dZoneManager.h"

#include <stdio.h>  /* defines FILENAME_MAX */
#ifdef _WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include "Metrics.hpp"

#include "User.h"
#include "UserManager.h"
#include "BitStream.h"
#include "dCommonVars.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "EntityManager.h"
#include "dLogger.h"
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
#include "PacketUtils.h"
#include "Loot.h"
#include "EntityInfo.h"
#include "LUTriggers.h"
#include "Player.h"
#include "PhantomPhysicsComponent.h"
#include "ProximityMonitorComponent.h"
#include "dpShapeSphere.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "VehiclePhysicsComponent.h"
#include "BuffComponent.h"
#include "SkillComponent.h"
#include "VanityUtilities.h"
#include "ScriptedActivityComponent.h"
#include "LevelProgressionComponent.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "dConfig.h"
#include "eBubbleType.h"
#include "AMFFormat.h"
#include "MovingPlatformComponent.h"
#include "dMessageIdentifiers.h"
#include "eMissionState.h"
#include "TriggerComponent.h"
#include "eServerDisconnectIdentifiers.h"
#include "eReplicaComponentType.h"

void SlashCommandHandler::HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr) {
	std::string chatCommand;
	std::vector<std::string> args;

	uint32_t breakIndex = 0;
	for (uint32_t i = 1; i < command.size(); ++i) {
		if (command[i] == L' ') {
			breakIndex = i;
			break;
		}

		chatCommand.push_back(static_cast<unsigned char>(command[i]));
		breakIndex++;
	}

	uint32_t index = ++breakIndex;
	while (true) {
		std::string arg;

		while (index < command.size()) {
			if (command[index] == L' ') {
				args.push_back(arg);
				arg = "";
				index++;
				continue;
			}

			arg.push_back(static_cast<char>(command[index]));
			index++;
		}

		if (arg != "") {
			args.push_back(arg);
		}

		break;
	}

	//Game::logger->Log("SlashCommandHandler", "Received chat command \"%s\"", GeneralUtils::UTF16ToWTF8(command).c_str());

	User* user = UserManager::Instance()->GetUser(sysAddr);
	if ((chatCommand == "setgmlevel" || chatCommand == "makegm" || chatCommand == "gmlevel") && user->GetMaxGMLevel() > GAME_MASTER_LEVEL_CIVILIAN) {
		if (args.size() != 1) return;

		uint32_t level;

		if (!GeneralUtils::TryParse(args[0], level)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid gm level.");
			return;
		}

#ifndef DEVELOPER_SERVER
		if (user->GetMaxGMLevel() == GAME_MASTER_LEVEL_JUNIOR_DEVELOPER) {
			level = GAME_MASTER_LEVEL_CIVILIAN;
		}
#endif

		if (level > user->GetMaxGMLevel()) {
			level = user->GetMaxGMLevel();
		}

		if (level == entity->GetGMLevel()) return;
		bool success = user->GetMaxGMLevel() >= level;

		if (success) {

			if (entity->GetGMLevel() > GAME_MASTER_LEVEL_CIVILIAN && level == GAME_MASTER_LEVEL_CIVILIAN) {
				GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);
			} else if (entity->GetGMLevel() == GAME_MASTER_LEVEL_CIVILIAN && level > GAME_MASTER_LEVEL_CIVILIAN) {
				GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
			}

			WorldPackets::SendGMLevelChange(sysAddr, success, user->GetMaxGMLevel(), entity->GetGMLevel(), level);
			GameMessages::SendChatModeUpdate(entity->GetObjectID(), level);
			entity->SetGMLevel(level);
			Game::logger->Log("SlashCommandHandler", "User %s (%i) has changed their GM level to %i for charID %llu", user->GetUsername().c_str(), user->GetAccountID(), level, entity->GetObjectID());
		}
	}

#ifndef DEVELOPER_SERVER
	if ((entity->GetGMLevel() > user->GetMaxGMLevel()) || (entity->GetGMLevel() > GAME_MASTER_LEVEL_CIVILIAN && user->GetMaxGMLevel() == GAME_MASTER_LEVEL_JUNIOR_DEVELOPER)) {
		WorldPackets::SendGMLevelChange(sysAddr, true, user->GetMaxGMLevel(), entity->GetGMLevel(), GAME_MASTER_LEVEL_CIVILIAN);
		GameMessages::SendChatModeUpdate(entity->GetObjectID(), GAME_MASTER_LEVEL_CIVILIAN);
		entity->SetGMLevel(GAME_MASTER_LEVEL_CIVILIAN);

		GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);

		ChatPackets::SendSystemMessage(sysAddr, u"Your game master level has been changed, you may not be able to use all commands.");
	}
#endif

	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//HANDLE ALL NON GM SLASH COMMANDS RIGHT HERE!
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	if (chatCommand == "pvp") {
		auto* character = entity->GetComponent<CharacterComponent>();

		if (character == nullptr) {
			Game::logger->Log("SlashCommandHandler", "Failed to find character component!");
			return;
		}

		character->SetPvpEnabled(!character->GetPvpEnabled());
		EntityManager::Instance()->SerializeEntity(entity);

		std::stringstream message;
		message << character->GetName() << " changed their PVP flag to " << std::to_string(character->GetPvpEnabled()) << "!";

		ChatPackets::SendSystemMessage(UNASSIGNED_SYSTEM_ADDRESS, GeneralUtils::UTF8ToUTF16(message.str()), true);

		return;
	}

	if (chatCommand == "who") {
		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Players in this instance: (" + GeneralUtils::to_u16string(Player::GetAllPlayers().size()) + u")"
		);

		for (auto* player : Player::GetAllPlayers()) {
			const auto& name = player->GetCharacter()->GetName();

			ChatPackets::SendSystemMessage(
				sysAddr,
				GeneralUtils::UTF8ToUTF16(player == entity ? name + " (you)" : name)
			);
		}
	}

	if (chatCommand == "ping") {
		if (!args.empty() && args[0] == "-l") {
			std::stringstream message;
			message << "Your latest ping: " << std::to_string(Game::server->GetLatestPing(sysAddr)) << "ms";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		} else {
			std::stringstream message;
			message << "Your average ping: " << std::to_string(Game::server->GetPing(sysAddr)) << "ms";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		}
		return;
	}

	if (chatCommand == "fix-stats") {
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

	if (chatCommand == "credits" || chatCommand == "info") {
		const auto& customText = chatCommand == "credits" ? VanityUtilities::ParseMarkdown((BinaryPathFinder::GetBinaryDir() / "vanity/CREDITS.md").string()) : VanityUtilities::ParseMarkdown((BinaryPathFinder::GetBinaryDir() /  "vanity/INFO.md").string());

		{
			AMFArrayValue args;

			auto* state = new AMFStringValue();
			state->SetStringValue("Story");

			args.InsertValue("state", state);

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", &args);
		}

		entity->AddCallbackTimer(0.5f, [customText, entity]() {
			AMFArrayValue args;

			auto* text = new AMFStringValue();
			text->SetStringValue(customText);

			args.InsertValue("visible", new AMFTrueValue());
			args.InsertValue("text", text);

			Game::logger->Log("SlashCommandHandler", "Sending %s", customText.c_str());

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "ToggleStoryBox", &args);
			});

		return;
	}

	if (chatCommand == "leave-zone") {
		const auto currentZone = dZoneManager::Instance()->GetZone()->GetZoneID().GetMapID();

		auto newZone = 0;
		if (currentZone % 100 == 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"You are not in an instanced zone.");
			return;
		} else {
			newZone = (currentZone / 100) * 100;
		}
		// If new zone would be inaccessible, then default to Avant Gardens.
		if (!CheckIfAccessibleZone(newZone)) newZone = 1100;

		ChatPackets::SendSystemMessage(sysAddr, u"Leaving zone...");

		const auto objid = entity->GetObjectID();

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, newZone, 0, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			auto* entity = EntityManager::Instance()->GetEntity(objid);

			if (entity == nullptr) {
				return;
			}

			const auto sysAddr = entity->GetSystemAddress();

			Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", entity->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

			if (entity->GetCharacter()) {
				entity->GetCharacter()->SetZoneID(zoneID);
				entity->GetCharacter()->SetZoneInstance(zoneInstance);
				entity->GetCharacter()->SetZoneClone(zoneClone);
			}

			entity->GetCharacter()->SaveXMLToDatabase();

			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			});
	}

	if (chatCommand == "join" && !args.empty()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Requesting private map...");
		const auto& password = args[0];

		ZoneInstanceManager::Instance()->RequestPrivateZone(Game::server, false, password, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

			if (entity->GetCharacter()) {
				entity->GetCharacter()->SetZoneID(zoneID);
				entity->GetCharacter()->SetZoneInstance(zoneInstance);
				entity->GetCharacter()->SetZoneClone(zoneClone);
			}

			entity->GetCharacter()->SaveXMLToDatabase();

			WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			});
	}

	if (user->GetMaxGMLevel() == 0 || entity->GetGMLevel() >= 0) {
		if (chatCommand == "die") {
			entity->Smash(entity->GetObjectID());
		}

		if (chatCommand == "resurrect") {
			ScriptedActivityComponent* scriptedActivityComponent = dZoneManager::Instance()->GetZoneControlObject()->GetComponent<ScriptedActivityComponent>();

			if (scriptedActivityComponent) { // check if user is in activity world and if so, they can't resurrect
				ChatPackets::SendSystemMessage(sysAddr, u"You cannot resurrect in an activity world.");
				return;
			}

			GameMessages::SendResurrect(entity);
		}

		if (chatCommand == "requestmailcount") {
			Mail::HandleNotificationRequest(entity->GetSystemAddress(), entity->GetObjectID());
		}

		if (chatCommand == "instanceinfo") {
			const auto zoneId = dZoneManager::Instance()->GetZone()->GetZoneID();

			ChatPackets::SendSystemMessage(sysAddr, u"Map: " + (GeneralUtils::to_u16string(zoneId.GetMapID())) + u"\nClone: " + (GeneralUtils::to_u16string(zoneId.GetCloneID())) + u"\nInstance: " + (GeneralUtils::to_u16string(zoneId.GetInstanceID())));
		}

		if (entity->GetGMLevel() == 0) return;
	}

	// Log command to database
	auto stmt = Database::CreatePreppedStmt("INSERT INTO command_log (character_id, command) VALUES (?, ?);");
	stmt->setInt(1, entity->GetCharacter()->GetID());
	stmt->setString(2, GeneralUtils::UTF16ToWTF8(command).c_str());
	stmt->execute();
	delete stmt;

	if (chatCommand == "setminifig" && args.size() == 2 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_FORUM_MODERATOR) { // could break characters so only allow if GM > 0
		int32_t minifigItemId;
		if (!GeneralUtils::TryParse(args[1], minifigItemId)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid Minifig Item Id ID.");
			return;
		}
		EntityManager::Instance()->DestructEntity(entity, sysAddr);
		auto* charComp = entity->GetComponent<CharacterComponent>();
		std::string lowerName = args[0];
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
			EntityManager::Instance()->ConstructEntity(entity);
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid Minifig item to change, try one of the following: Eyebrows, Eyes, HairColor, HairStyle, Pants, LeftHand, Mouth, RightHand, Shirt, Hands");
			return;
		}

		EntityManager::Instance()->ConstructEntity(entity);
		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(lowerName) + u" set to " + (GeneralUtils::to_u16string(minifigItemId)));

		GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS); // need to retoggle because it gets reenabled on creation of new character
	}

	if ((chatCommand == "playanimation" || chatCommand == "playanim") && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		std::u16string anim = GeneralUtils::ASCIIToUTF16(args[0], args[0].size());
		GameMessages::SendPlayAnimation(entity, anim);
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto* possessedComponent = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());
			if (possessedComponent) GameMessages::SendPlayAnimation(possessedComponent, anim);
		}
	}

	if (chatCommand == "list-spawns" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		for (const auto& pair : EntityManager::Instance()->GetSpawnPointEntities()) {
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(pair.first));
		}

		ChatPackets::SendSystemMessage(sysAddr, u"Current: " + GeneralUtils::ASCIIToUTF16(entity->GetCharacter()->GetTargetScene()));

		return;
	}

	if (chatCommand == "unlock-emote" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t emoteID;

		if (!GeneralUtils::TryParse(args[0], emoteID)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid emote ID.");
			return;
		}

		entity->GetCharacter()->UnlockEmote(emoteID);
	}

	if (chatCommand == "force-save" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		entity->GetCharacter()->SaveXMLToDatabase();
	}

	if (chatCommand == "kill" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		ChatPackets::SendSystemMessage(sysAddr, u"Brutally murdering that player, if online on this server.");

		auto* user = UserManager::Instance()->GetUser(args[0]);
		if (user) {
			auto* player = EntityManager::Instance()->GetEntity(user->GetLoggedInChar());
			player->Smash(entity->GetObjectID());
			ChatPackets::SendSystemMessage(sysAddr, u"It has been done, do you feel good about yourself now?");
			return;
		}

		ChatPackets::SendSystemMessage(sysAddr, u"They were saved from your carnage.");
		return;
	}

	if (chatCommand == "speedboost" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		float boost;

		if (!GeneralUtils::TryParse(args[0], boost)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid boost.");
			return;
		}

		auto* controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();

		if (!controllablePhysicsComponent) return;
		controllablePhysicsComponent->SetSpeedMultiplier(boost);

		// speedboost possesables
		auto possessor = entity->GetComponent<PossessorComponent>();
		if (possessor) {
			auto possessedID = possessor->GetPossessable();
			if (possessedID != LWOOBJID_EMPTY) {
				auto possessable = EntityManager::Instance()->GetEntity(possessedID);
				if (possessable) {
					auto* possessControllablePhysicsComponent = possessable->GetComponent<ControllablePhysicsComponent>();
					if (possessControllablePhysicsComponent) {
						possessControllablePhysicsComponent->SetSpeedMultiplier(boost);
					}
				}
			}
		}

		EntityManager::Instance()->SerializeEntity(entity);
	}

	if (chatCommand == "freecam" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto state = !entity->GetVar<bool>(u"freecam");
		entity->SetVar<bool>(u"freecam", state);

		GameMessages::SendSetPlayerControlScheme(entity, static_cast<eControlSceme>(state ? 9 : 1));

		ChatPackets::SendSystemMessage(sysAddr, u"Toggled freecam.");
		return;
	}

	if (chatCommand == "setcontrolscheme" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		uint32_t scheme;

		if (!GeneralUtils::TryParse(args[0], scheme)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid control scheme.");
			return;
		}

		GameMessages::SendSetPlayerControlScheme(entity, static_cast<eControlSceme>(scheme));

		ChatPackets::SendSystemMessage(sysAddr, u"Switched control scheme.");
		return;
	}

	if (chatCommand == "approveproperty" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR) {

		if (PropertyManagementComponent::Instance() != nullptr) {
			PropertyManagementComponent::Instance()->UpdateApprovedStatus(true);
		}

		return;
	}

	if (chatCommand == "setuistate" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		AMFStringValue* value = new AMFStringValue();
		value->SetStringValue(args[0]);

		AMFArrayValue args;
		args.InsertValue("state", value);
		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "pushGameState", &args);

		ChatPackets::SendSystemMessage(sysAddr, u"Switched UI state.");

		return;
	}

	if (chatCommand == "toggle" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		AMFTrueValue* value = new AMFTrueValue();

		AMFArrayValue amfArgs;
		amfArgs.InsertValue("visible", value);
		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, args[0], &amfArgs);

		ChatPackets::SendSystemMessage(sysAddr, u"Toggled UI state.");

		return;
	}

	if ((chatCommand == "setinventorysize" || chatCommand == "setinvsize") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		uint32_t size;

		if (!GeneralUtils::TryParse(args.at(0), size)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid size.");
			return;
		}

		eInventoryType selectedInventory = eInventoryType::ITEMS;

		// a possible inventory was provided if we got more than 1 argument
		if (args.size() >= 2) {
			selectedInventory = eInventoryType::INVALID;
			if (!GeneralUtils::TryParse(args.at(1), selectedInventory)) {
				// In this case, we treat the input as a string and try to find it in the reflection list
				std::transform(args.at(1).begin(), args.at(1).end(), args.at(1).begin(), ::toupper);
				for (uint32_t index = 0; index < NUMBER_OF_INVENTORIES; index++) {
					if (std::string_view(args.at(1)) == std::string_view(InventoryType::InventoryTypeToString(static_cast<eInventoryType>(index)))) selectedInventory = static_cast<eInventoryType>(index);
				}
			}
			if (selectedInventory == eInventoryType::INVALID) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid inventory.");
				return;
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Setting inventory " +
				GeneralUtils::ASCIIToUTF16(args.at(1)) +
				u" to size " +
				GeneralUtils::to_u16string(size));
		} else ChatPackets::SendSystemMessage(sysAddr, u"Setting inventory ITEMS to size " + GeneralUtils::to_u16string(size));

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			auto* inventory = inventoryComponent->GetInventory(selectedInventory);

			inventory->SetSize(size);
		}

		return;
	}

	if (chatCommand == "runmacro" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() != 1) return;

		// Only process if input does not contain separator charaters
		if (args[0].find("/") != std::string::npos) return;
		if (args[0].find("\\") != std::string::npos) return;

		auto buf = Game::assetManager->GetFileAsBuffer(("macros/" + args[0] + ".scm").c_str());

		if (!buf.m_Success) {
			ChatPackets::SendSystemMessage(sysAddr, u"Unknown macro! Is the filename right?");
			return;
		}

		std::istream infile(&buf);

		if (infile.good()) {
			std::string line;
			while (std::getline(infile, line)) {
				SlashCommandHandler::HandleChatCommand(GeneralUtils::ASCIIToUTF16(line), entity, sysAddr);
			}
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Unknown macro! Is the filename right?");
		}

		buf.close();

		return;
	}

	if (chatCommand == "addmission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}

		auto comp = static_cast<MissionComponent*>(entity->GetComponent(eReplicaComponentType::MISSION));
		if (comp) comp->AcceptMission(missionID, true);
		return;
	}

	if (chatCommand == "completemission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}

		auto comp = static_cast<MissionComponent*>(entity->GetComponent(eReplicaComponentType::MISSION));
		if (comp) comp->CompleteMission(missionID, true);
		return;
	}

	if (chatCommand == "setflag" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1) {
		uint32_t flagId;

		if (!GeneralUtils::TryParse(args[0], flagId)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}

		entity->GetCharacter()->SetPlayerFlag(flagId, true);
	}

	if (chatCommand == "setflag" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 2) {
		uint32_t flagId;
		std::string onOffFlag = args[0];
		if (!GeneralUtils::TryParse(args[1], flagId)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}
		if (onOffFlag != "off" && onOffFlag != "on") {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag type.");
			return;
		}
		entity->GetCharacter()->SetPlayerFlag(flagId, onOffFlag == "on");
	}
	if (chatCommand == "clearflag" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1) {
		uint32_t flagId;

		if (!GeneralUtils::TryParse(args[0], flagId)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}

		entity->GetCharacter()->SetPlayerFlag(flagId, false);
	}

	if (chatCommand == "resetmission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}

		auto* comp = static_cast<MissionComponent*>(entity->GetComponent(eReplicaComponentType::MISSION));

		if (comp == nullptr) {
			return;
		}

		auto* mission = comp->GetMission(missionID);

		if (mission == nullptr) {
			return;
		}

		mission->SetMissionState(eMissionState::ACTIVE);

		return;
	}

	if (chatCommand == "playeffect" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 3) {
		int32_t effectID = 0;

		if (!GeneralUtils::TryParse(args[0], effectID)) {
			return;
		}

		// FIXME: use fallible ASCIIToUTF16 conversion, because non-ascii isn't valid anyway
		GameMessages::SendPlayFXEffect(entity->GetObjectID(), effectID, GeneralUtils::ASCIIToUTF16(args[1]), args[2]);
	}

	if (chatCommand == "stopeffect" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		GameMessages::SendStopFXEffect(entity, true, args[0]);
	}

	if (chatCommand == "setanntitle" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() < 0) return;

		std::stringstream ss;
		for (auto string : args)
			ss << string << " ";

		entity->GetCharacter()->SetAnnouncementTitle(ss.str());
		return;
	}

	if (chatCommand == "setannmsg" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() < 0) return;

		std::stringstream ss;
		for (auto string : args)
			ss << string << " ";

		entity->GetCharacter()->SetAnnouncementMessage(ss.str());
		return;
	}

	if (chatCommand == "announce" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (entity->GetCharacter()->GetAnnouncementTitle().size() == 0 || entity->GetCharacter()->GetAnnouncementMessage().size() == 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"Use /setanntitle <title> & /setannmsg <msg> first!");
			return;
		}

		SendAnnouncement(entity->GetCharacter()->GetAnnouncementTitle(), entity->GetCharacter()->GetAnnouncementMessage());
		return;
	}

	if (chatCommand == "shutdownuniverse" && entity->GetGMLevel() == GAME_MASTER_LEVEL_OPERATOR) {
		//Tell the master server that we're going to be shutting down whole "universe":
		CBITSTREAM;
		PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_SHUTDOWN_UNIVERSE);
		Game::server->SendToMaster(&bitStream);
		ChatPackets::SendSystemMessage(sysAddr, u"Sent universe shutdown notification to master.");

		//Tell chat to send an announcement to all servers
		SendAnnouncement("Servers Closing Soon!", "DLU servers will close for maintenance in 10 minutes from now.");
		return;
	}

	if (chatCommand == "getnavmeshheight" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto control = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
		if (!control) return;

		float y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(control->GetPosition());
		std::u16string msg = u"Navmesh height: " + (GeneralUtils::to_u16string(y));
		ChatPackets::SendSystemMessage(sysAddr, msg);
	}

	if (chatCommand == "gmadditem" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 1) {
			uint32_t itemLOT;

			if (!GeneralUtils::TryParse(args[0], itemLOT)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}

			InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));

			inventory->AddItem(itemLOT, 1, eLootSourceType::LOOT_SOURCE_MODERATION);
		} else if (args.size() == 2) {
			uint32_t itemLOT;

			if (!GeneralUtils::TryParse(args[0], itemLOT)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}

			uint32_t count;

			if (!GeneralUtils::TryParse(args[1], count)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item count.");
				return;
			}

			InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));

			inventory->AddItem(itemLOT, count, eLootSourceType::LOOT_SOURCE_MODERATION);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /gmadditem <lot>");
		}
	}

	if (chatCommand == "mailitem" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_MODERATOR && args.size() >= 2) {
		const auto& playerName = args[0];

		sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id from charinfo WHERE name=? LIMIT 1;");
		stmt->setString(1, playerName);
		sql::ResultSet* res = stmt->executeQuery();
		uint32_t receiverID = 0;

		if (res->rowsCount() > 0) {
			while (res->next()) receiverID = res->getUInt(1);
		}

		delete stmt;
		delete res;

		if (receiverID == 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to find that player");

			return;
		}

		uint32_t lot;

		if (!GeneralUtils::TryParse(args[1], lot)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid item lot.");
			return;
		}

		uint64_t currentTime = time(NULL);
		sql::PreparedStatement* ins = Database::CreatePreppedStmt("INSERT INTO `mail`(`sender_id`, `sender_name`, `receiver_id`, `receiver_name`, `time_sent`, `subject`, `body`, `attachment_id`, `attachment_lot`, `attachment_subkey`, `attachment_count`, `was_read`) VALUES (?,?,?,?,?,?,?,?,?,?,?,0)");
		ins->setUInt(1, entity->GetObjectID());
		ins->setString(2, "Darkflame Universe");
		ins->setUInt(3, receiverID);
		ins->setString(4, playerName);
		ins->setUInt64(5, currentTime);
		ins->setString(6, "Lost item");
		ins->setString(7, "This is a replacement item for one you lost.");
		ins->setUInt(8, 0);
		ins->setInt(9, lot);
		ins->setInt(10, 0);
		ins->setInt(11, 1);
		ins->execute();
		delete ins;

		ChatPackets::SendSystemMessage(sysAddr, u"Mail sent");

		return;
	}

	if (chatCommand == "setname" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		std::string name = "";

		for (const auto& arg : args) {
			name += arg + " ";
		}

		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
	}

	if (chatCommand == "title" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		std::string name = entity->GetCharacter()->GetName() + " - ";

		for (const auto& arg : args) {
			name += arg + " ";
		}

		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::UTF8ToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
	}

	if ((chatCommand == "teleport" || chatCommand == "tele") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_MODERATOR) {
		NiPoint3 pos{};
		if (args.size() == 3) {

			float x, y, z;

			if (!GeneralUtils::TryParse(args[0], x)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}

			if (!GeneralUtils::TryParse(args[1], y)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid y.");
				return;
			}

			if (!GeneralUtils::TryParse(args[2], z)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

			pos.SetX(x);
			pos.SetY(y);
			pos.SetZ(z);

			Game::logger->Log("SlashCommandHandler", "Teleporting objectID: %llu to %f, %f, %f", entity->GetObjectID(), pos.x, pos.y, pos.z);
			GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
		} else if (args.size() == 2) {

			float x, z;

			if (!GeneralUtils::TryParse(args[0], x)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}

			if (!GeneralUtils::TryParse(args[1], z)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

			pos.SetX(x);
			pos.SetY(0.0f);
			pos.SetZ(z);

			Game::logger->Log("SlashCommandHandler", "Teleporting objectID: %llu to X: %f, Z: %f", entity->GetObjectID(), pos.x, pos.z);
			GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /teleport <x> (<y>) <z> - if no Y given, will teleport to the height of the terrain (or any physics object).");
		}


		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto* possassableEntity = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

			if (possassableEntity != nullptr) {
				auto* vehiclePhysicsComponent = possassableEntity->GetComponent<VehiclePhysicsComponent>();
				if (vehiclePhysicsComponent) {
					vehiclePhysicsComponent->SetPosition(pos);
					EntityManager::Instance()->SerializeEntity(possassableEntity);
				} else GameMessages::SendTeleport(possassableEntity->GetObjectID(), pos, NiQuaternion(), sysAddr);
			}
		}
	}

	if (chatCommand == "tpall" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto pos = entity->GetPosition();

		const auto characters = EntityManager::Instance()->GetEntitiesByComponent(eReplicaComponentType::CHARACTER);

		for (auto* character : characters) {
			GameMessages::SendTeleport(character->GetObjectID(), pos, NiQuaternion(), character->GetSystemAddress());
		}

		return;
	}

	if (chatCommand == "dismount" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		if (possessorComponent) {
			auto possessableId = possessorComponent->GetPossessable();
			if (possessableId != LWOOBJID_EMPTY) {
				auto* possessableEntity = EntityManager::Instance()->GetEntity(possessableId);
				if (possessableEntity) possessorComponent->Dismount(possessableEntity, true);
			}
		}
	}

	if (chatCommand == "fly" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_DEVELOPER) {
		auto* character = entity->GetCharacter();

		if (character) {
			bool isFlying = character->GetIsFlying();

			if (isFlying) {
				GameMessages::SendSetJetPackMode(entity, false);

				character->SetIsFlying(false);
			} else {
				float speedScale = 1.0f;

				if (args.size() >= 1) {
					float tempScaleStore;

					if (GeneralUtils::TryParse<float>(args[0], tempScaleStore)) {
						speedScale = tempScaleStore;
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

	//------- GM COMMANDS TO ACTUALLY MODERATE --------

	if (chatCommand == "mute" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_DEVELOPER) {
		if (args.size() >= 1) {
			auto* player = Player::GetPlayer(args[0]);

			uint32_t accountId = 0;
			LWOOBJID characterId = 0;

			if (player == nullptr) {
				auto* accountQuery = Database::CreatePreppedStmt("SELECT account_id, id FROM charinfo WHERE name=? LIMIT 1;");

				accountQuery->setString(1, args[0]);

				auto result = accountQuery->executeQuery();

				if (result->rowsCount() > 0) {
					while (result->next()) {
						accountId = result->getUInt(1);
						characterId = result->getUInt64(2);

						characterId = GeneralUtils::SetBit(characterId, OBJECT_BIT_CHARACTER);
						characterId = GeneralUtils::SetBit(characterId, OBJECT_BIT_PERSISTENT);
					}
				}

				delete accountQuery;
				delete result;

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(args[0]));

					return;
				}
			} else {
				accountId = player->GetParentUser()->GetAccountID();
				characterId = player->GetCharacter()->GetID();
			}

			auto* userUpdate = Database::CreatePreppedStmt("UPDATE accounts SET mute_expire = ? WHERE id = ?;");

			time_t expire = 1; // Default to indefinate mute

			if (args.size() >= 2) {
				uint32_t days = 0;
				uint32_t hours = 0;
				if (!GeneralUtils::TryParse(args[1], days)) {
					ChatPackets::SendSystemMessage(sysAddr, u"Invalid days.");

					return;
				}

				if (args.size() >= 3) {
					if (!GeneralUtils::TryParse(args[2], hours)) {
						ChatPackets::SendSystemMessage(sysAddr, u"Invalid hours.");

						return;
					}
				}

				expire = time(NULL);
				expire += 24 * 60 * 60 * days;
				expire += 60 * 60 * hours;
			}

			userUpdate->setUInt64(1, expire);
			userUpdate->setInt(2, accountId);

			userUpdate->executeUpdate();

			delete userUpdate;

			char buffer[32] = "brought up for review.\0";

			if (expire != 1) {
				std::tm* ptm = std::localtime(&expire);
				// Format: Mo, 15.06.2009 20:20:00
				std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
			}

			const auto timeStr = GeneralUtils::ASCIIToUTF16(std::string(buffer));

			ChatPackets::SendSystemMessage(sysAddr, u"Muted: " + GeneralUtils::UTF8ToUTF16(args[0]) + u" until " + timeStr);

			//Notify chat about it
			CBITSTREAM;
			PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_MUTE_UPDATE);

			bitStream.Write(characterId);
			bitStream.Write(expire);

			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /mute <username> <days (optional)> <hours (optional)>");
		}
	}

	if (chatCommand == "kick" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_MODERATOR) {
		if (args.size() == 1) {
			auto* player = Player::GetPlayer(args[0]);

			std::u16string username = GeneralUtils::UTF8ToUTF16(args[0]);
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

	if (chatCommand == "ban" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_SENIOR_MODERATOR) {
		if (args.size() == 1) {
			auto* player = Player::GetPlayer(args[0]);

			uint32_t accountId = 0;

			if (player == nullptr) {
				auto* accountQuery = Database::CreatePreppedStmt("SELECT account_id FROM charinfo WHERE name=? LIMIT 1;");

				accountQuery->setString(1, args[0]);

				auto result = accountQuery->executeQuery();

				if (result->rowsCount() > 0) {
					while (result->next()) accountId = result->getUInt(1);
				}

				delete accountQuery;
				delete result;

				if (accountId == 0) {
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::UTF8ToUTF16(args[0]));

					return;
				}
			} else {
				accountId = player->GetParentUser()->GetAccountID();
			}

			auto* userUpdate = Database::CreatePreppedStmt("UPDATE accounts SET banned = true WHERE id = ?;");

			userUpdate->setInt(1, accountId);

			userUpdate->executeUpdate();

			delete userUpdate;

			if (player != nullptr) {
				Game::server->Disconnect(player->GetSystemAddress(), eServerDisconnectIdentifiers::FREE_TRIAL_EXPIRED);
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Banned: " + GeneralUtils::ASCIIToUTF16(args[0]));
		} else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /ban <username>");
		}
	}

	//-------------------------------------------------

	if (chatCommand == "buffme" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(999);
			dest->SetMaxHealth(999.0f);
			dest->SetArmor(999);
			dest->SetMaxArmor(999.0f);
			dest->SetImagination(999);
			dest->SetMaxImagination(999.0f);
		}
		EntityManager::Instance()->SerializeEntity(entity);
	}

	if (chatCommand == "startcelebration" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1) {
		int32_t celebration;

		if (!GeneralUtils::TryParse(args[0], celebration)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid celebration.");
			return;
		}

		GameMessages::SendStartCelebrationEffect(entity, entity->GetSystemAddress(), celebration);
	}

	if (chatCommand == "buffmed" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(9);
			dest->SetMaxHealth(9.0f);
			dest->SetArmor(9);
			dest->SetMaxArmor(9.0f);
			dest->SetImagination(9);
			dest->SetMaxImagination(9.0f);
		}
		EntityManager::Instance()->SerializeEntity(entity);
	}

	if (chatCommand == "refillstats" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {

		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth((int)dest->GetMaxHealth());
			dest->SetArmor((int)dest->GetMaxArmor());
			dest->SetImagination((int)dest->GetMaxImagination());
		}

		EntityManager::Instance()->SerializeEntity(entity);
	}

	if (chatCommand == "lookup" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		auto query = CDClientDatabase::CreatePreppedStmt(
			"SELECT `id`, `name` FROM `Objects` WHERE `displayName` LIKE ?1 OR `name` LIKE ?1 OR `description` LIKE ?1 LIMIT 50");
		// Concatenate all of the arguments into a single query so a multi word query can be used properly.
		std::string conditional = args[0];
		args.erase(args.begin());
		for (auto& argument : args) {
			conditional += ' ' + argument;
		}

		const std::string query_text = "%" + conditional + "%";
		query.bind(1, query_text.c_str());

		auto tables = query.execQuery();

		while (!tables.eof()) {
			std::string message = std::to_string(tables.getIntField(0)) + " - " + tables.getStringField(1);
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(message, message.size()));
			tables.nextRow();
		}
	}

	if (chatCommand == "spawn" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		ControllablePhysicsComponent* comp = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
		if (!comp) return;

		uint32_t lot;

		if (!GeneralUtils::TryParse(args[0], lot)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid lot.");
			return;
		}

		EntityInfo info;
		info.lot = lot;
		info.pos = comp->GetPosition();
		info.rot = comp->GetRotation();
		info.spawner = nullptr;
		info.spawnerID = entity->GetObjectID();
		info.spawnerNodeID = 0;

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);

		if (newEntity == nullptr) {
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to spawn entity.");
			return;
		}

		auto vehiclePhysicsComponent = newEntity->GetComponent<VehiclePhysicsComponent>();
		if (vehiclePhysicsComponent) {
			auto newRot = newEntity->GetRotation();
			auto angles = newRot.GetEulerAngles();
			// make it right side up
			angles.x -= PI;
			// make it going in the direction of the player
			angles.y -= PI;
			newRot = NiQuaternion::FromEulerAngles(angles);
			newEntity->SetRotation(newRot);
		}

		EntityManager::Instance()->ConstructEntity(newEntity);
	}

	if (chatCommand == "spawngroup" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 3) {
		auto controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();
		if (!controllablePhysicsComponent) return;

		LOT lot{};
		uint32_t numberToSpawn{};
		float radiusToSpawnWithin{};

		if (!GeneralUtils::TryParse(args[0], lot)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid lot.");
			return;
		}

		if (!GeneralUtils::TryParse(args[1], numberToSpawn) && numberToSpawn > 0) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid number of enemies to spawn.");
			return;
		}

		// Must spawn within a radius of at least 0.0f
		if (!GeneralUtils::TryParse(args[2], radiusToSpawnWithin) && radiusToSpawnWithin < 0.0f) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid radius to spawn within.");
			return;
		}

		EntityInfo info;
		info.lot = lot;
		info.spawner = nullptr;
		info.spawnerID = entity->GetObjectID();
		info.spawnerNodeID = 0;

		auto playerPosition = controllablePhysicsComponent->GetPosition();
		while (numberToSpawn > 0) {
			auto randomAngle = GeneralUtils::GenerateRandomNumber<float>(0.0f, 2 * PI);
			auto randomRadius = GeneralUtils::GenerateRandomNumber<float>(0.0f, radiusToSpawnWithin);

			// Set the position to the generated random position plus the player position.  This will
			// spawn the entity in a circle around the player.  As you get further from the player, the angle chosen will get less accurate.
			info.pos = playerPosition + NiPoint3(cos(randomAngle) * randomRadius, 0.0f, sin(randomAngle) * randomRadius);
			info.rot = NiQuaternion();

			auto newEntity = EntityManager::Instance()->CreateEntity(info);
			if (newEntity == nullptr) {
				ChatPackets::SendSystemMessage(sysAddr, u"Failed to spawn entity.");
				return;
			}

			EntityManager::Instance()->ConstructEntity(newEntity);
			numberToSpawn--;
		}
	}

	if ((chatCommand == "giveuscore") && args.size() >= 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t uscore;

		if (!GeneralUtils::TryParse(args[0], uscore)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid uscore.");
			return;
		}

		CharacterComponent* character = entity->GetComponent<CharacterComponent>();
		if (character) character->SetUScore(character->GetUScore() + uscore);
		// LOOT_SOURCE_MODERATION should work but it doesn't.  Relog to see uscore changes

		eLootSourceType lootType = eLootSourceType::LOOT_SOURCE_MODERATION;

		int32_t type;
		if (args.size() >= 2 && GeneralUtils::TryParse(args[1], type)) {
			lootType = (eLootSourceType)type;
		}

		GameMessages::SendModifyLEGOScore(entity, entity->GetSystemAddress(), uscore, lootType);
	}

	if ((chatCommand == "setlevel") && args.size() >= 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		// We may be trying to set a specific players level to a level.  If so override the entity with the requested players.
		std::string requestedPlayerToSetLevelOf = "";
		if (args.size() > 1) {
			requestedPlayerToSetLevelOf = args[1];

			auto requestedPlayer = Player::GetPlayer(requestedPlayerToSetLevelOf);

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
		uint32_t requestedLevel;
		uint32_t oldLevel;
		// first check the level is valid

		if (!GeneralUtils::TryParse(args[0], requestedLevel)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid level.");
			return;
		}
		// query to set our uscore to the correct value for this level

		auto characterComponent = entity->GetComponent<CharacterComponent>();
		if (!characterComponent) return;
		auto levelComponent = entity->GetComponent<LevelProgressionComponent>();
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT requiredUScore from LevelProgressionLookup WHERE id = ?;");
		query.bind(1, (int)requestedLevel);
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
		return;
	}

	if (chatCommand == "pos" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto position = entity->GetPosition();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(position.x)) + u", " + (GeneralUtils::to_u16string(position.y)) + u", " + (GeneralUtils::to_u16string(position.z)) + u">");

		std::cout << position.x << ", " << position.y << ", " << position.z << std::endl;
	}

	if (chatCommand == "rot" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto rotation = entity->GetRotation();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(rotation.w)) + u", " + (GeneralUtils::to_u16string(rotation.x)) + u", " + (GeneralUtils::to_u16string(rotation.y)) + u", " + (GeneralUtils::to_u16string(rotation.z)) + u">");

		std::cout << rotation.w << ", " << rotation.x << ", " << rotation.y << ", " << rotation.z << std::endl;
	}

	if (chatCommand == "locrow" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto position = entity->GetPosition();
		const auto rotation = entity->GetRotation();

		std::cout << "<location x=\"" << position.x << "\" y=\"" << position.y << "\" z=\"" << position.z << "\" rw=\"" << rotation.w << "\" rx=\"" << rotation.x << "\" ry=\"" << rotation.y << "\" rz=\"" << rotation.z << "\" />" << std::endl;
	}

	if (chatCommand == "playlvlfx" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		GameMessages::SendPlayFXEffect(entity, 7074, u"create", "7074", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}

	if (chatCommand == "playrebuildfx" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		GameMessages::SendPlayFXEffect(entity, 230, u"rebuild", "230", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}

	if ((chatCommand == "freemoney" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) && args.size() == 1) {
		int32_t money;

		if (!GeneralUtils::TryParse(args[0], money)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(ch->GetCoins() + money, eLootSourceType::LOOT_SOURCE_MODERATION);
	}

	if ((chatCommand == "setcurrency") && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t money;

		if (!GeneralUtils::TryParse(args[0], money)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(money, eLootSourceType::LOOT_SOURCE_MODERATION);
	}

	// Allow for this on even while not a GM, as it sometimes toggles incorrrectly.
	if (chatCommand == "gminvis" && entity->GetParentUser()->GetMaxGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);

		return;
	}

	if (chatCommand == "gmimmune" && args.size() >= 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		int32_t state = false;

		if (!GeneralUtils::TryParse(args[0], state)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent != nullptr) {
			destroyableComponent->SetIsGMImmune(state);
		}

		return;
	}

	if (chatCommand == "buff" && args.size() >= 2 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* buffComponent = entity->GetComponent<BuffComponent>();

		int32_t id = 0;
		int32_t duration = 0;

		if (!GeneralUtils::TryParse(args[0], id)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff id.");
			return;
		}

		if (!GeneralUtils::TryParse(args[1], duration)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff duration.");
			return;
		}

		if (buffComponent != nullptr) {
			buffComponent->ApplyBuff(id, duration, entity->GetObjectID());
		}

		return;
	}

	if ((chatCommand == "testmap" && args.size() >= 1) && entity->GetGMLevel() >= GAME_MASTER_LEVEL_FORUM_MODERATOR) {
		ChatPackets::SendSystemMessage(sysAddr, u"Requesting map change...");
		uint32_t reqZone;
		LWOCLONEID cloneId = 0;
		bool force = false;

		if (!GeneralUtils::TryParse(args[0], reqZone)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}

		if (args.size() > 1) {
			auto index = 1;

			if (args[index] == "force") {
				index++;

				force = true;
			}

			if (args.size() > index && !GeneralUtils::TryParse(args[index], cloneId)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid clone id.");
				return;
			}
		}

		const auto objid = entity->GetObjectID();

		if (force || CheckIfAccessibleZone(reqZone)) { // to prevent tomfoolery

			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, reqZone, cloneId, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {

				auto* entity = EntityManager::Instance()->GetEntity(objid);
				if (!entity) return;

				const auto sysAddr = entity->GetSystemAddress();

				ChatPackets::SendSystemMessage(sysAddr, u"Transfering map...");

				Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
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
			msg.append(args[0]); // FIXME: unnecessary utf16 re-encoding just for error
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(msg, msg.size()));
		}
	}

	if (chatCommand == "createprivate" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 3) {
		uint32_t zone;

		if (!GeneralUtils::TryParse(args[0], zone)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}

		uint32_t clone;

		if (!GeneralUtils::TryParse(args[1], clone)) {
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid clone.");
			return;
		}

		const auto& password = args[2];

		ZoneInstanceManager::Instance()->CreatePrivateZone(Game::server, zone, clone, password);

		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16("Sent request for private zone with password: " + password));

		return;
	}

	if ((chatCommand == "debugui") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		ChatPackets::SendSystemMessage(sysAddr, u"Opening UIDebugger...");
		AMFArrayValue args;
		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "ToggleUIDebugger;", nullptr);
	}

	if ((chatCommand == "boost") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		auto* vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		if (args.size() >= 1) {
			float time;

			if (!GeneralUtils::TryParse(args[0], time)) {
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid boost time.");
				return;
			} else {
				GameMessages::SendVehicleAddPassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
				entity->AddCallbackTimer(time, [vehicle]() {
					if (!vehicle) return;
					GameMessages::SendVehicleRemovePassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
					});
			}
		} else {
			GameMessages::SendVehicleAddPassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		}

	}

	if ((chatCommand == "unboost") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) return;
		auto* vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) return;
		GameMessages::SendVehicleRemovePassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	}

	if (chatCommand == "activatespawner" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(args[0]);

		for (auto* spawner : spawners) {
			spawner->Activate();
		}

		spawners = dZoneManager::Instance()->GetSpawnersInGroup(args[0]);

		for (auto* spawner : spawners) {
			spawner->Activate();
		}
	}

	if (chatCommand == "spawnphysicsverts" && entity->GetGMLevel() >= 6) {
		//Go tell physics to spawn all the vertices:
		auto entities = EntityManager::Instance()->GetEntitiesByComponent(eReplicaComponentType::PHANTOM_PHYSICS);
		for (auto en : entities) {
			auto phys = static_cast<PhantomPhysicsComponent*>(en->GetComponent(eReplicaComponentType::PHANTOM_PHYSICS));
			if (phys)
				phys->SpawnVertices();
		}
	}

	if (chatCommand == "reportproxphys" && entity->GetGMLevel() >= 6) {
		auto entities = EntityManager::Instance()->GetEntitiesByComponent(eReplicaComponentType::PROXIMITY_MONITOR);
		for (auto en : entities) {
			auto phys = static_cast<ProximityMonitorComponent*>(en->GetComponent(eReplicaComponentType::PROXIMITY_MONITOR));
			if (phys) {
				for (auto prox : phys->GetProximitiesData()) {
					if (!prox.second) continue;

					auto sphere = static_cast<dpShapeSphere*>(prox.second->GetShape());
					auto pos = prox.second->GetPosition();
					std::cout << prox.first << ", r: " << sphere->GetRadius() << ", pos: " << pos.x << "," << pos.y << "," << pos.z << std::endl;
				}
			}
		}
	}

	if (chatCommand == "triggerspawner" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(args[0]);

		for (auto* spawner : spawners) {
			spawner->Spawn();
		}

		spawners = dZoneManager::Instance()->GetSpawnersInGroup(args[0]);

		for (auto* spawner : spawners) {
			spawner->Spawn();
		}
	}

	if (chatCommand == "reforge" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 2) {
		LOT baseItem;
		LOT reforgedItem;

		if (!GeneralUtils::TryParse(args[0], baseItem)) return;
		if (!GeneralUtils::TryParse(args[1], reforgedItem)) return;

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (inventoryComponent == nullptr) return;

		std::vector<LDFBaseData*> data{};
		data.push_back(new LDFData<int32_t>(u"reforgedLOT", reforgedItem));

		inventoryComponent->AddItem(baseItem, 1, eLootSourceType::LOOT_SOURCE_MODERATION, eInventoryType::INVALID, data);
	}

	if (chatCommand == "crash" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR) {
		ChatPackets::SendSystemMessage(sysAddr, u"Crashing...");

		int* badPtr = nullptr;
		*badPtr = 0;

		return;
	}

	if (chatCommand == "metrics" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
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
			u"Peak RSS: " + GeneralUtils::to_u16string((float)((double)Metrics::GetPeakRSS() / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Current RSS: " + GeneralUtils::to_u16string((float)((double)Metrics::GetCurrentRSS() / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Process ID: " + GeneralUtils::to_u16string(Metrics::GetProcessID())
		);

		return;
	}

	if (chatCommand == "reloadconfig" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		Game::config->ReloadConfig();
		VanityUtilities::SpawnVanity();
		dpWorld::Instance().Reload();
		auto entities = EntityManager::Instance()->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
		for (auto entity : entities) {
			auto* scriptedActivityComponent = entity->GetComponent<ScriptedActivityComponent>();
			if (!scriptedActivityComponent) continue;

			scriptedActivityComponent->ReloadConfig();
		}
		Game::server->UpdateMaximumMtuSize();
		Game::server->UpdateBandwidthLimit();
		ChatPackets::SendSystemMessage(sysAddr, u"Successfully reloaded config for world!");
	}

	if (chatCommand == "rollloot" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR && args.size() >= 3) {
		uint32_t lootMatrixIndex = 0;
		uint32_t targetLot = 0;
		uint32_t loops = 1;

		if (!GeneralUtils::TryParse(args[0], lootMatrixIndex)) return;
		if (!GeneralUtils::TryParse(args[1], targetLot)) return;
		if (!GeneralUtils::TryParse(args[2], loops)) return;

		uint64_t totalRuns = 0;

		for (uint32_t i = 0; i < loops; i++) {
			while (true) {
				auto lootRoll = LootGenerator::Instance().RollLootMatrix(lootMatrixIndex);
				totalRuns += 1;
				bool doBreak = false;
				for (const auto& kv : lootRoll) {
					if ((uint32_t)kv.first == targetLot) {
						doBreak = true;
					}
				}
				if (doBreak) break;
			}
		}

		std::u16string message = u"Ran loot drops looking for "
			+ GeneralUtils::to_u16string(targetLot)
			+ u", "
			+ GeneralUtils::to_u16string(loops)
			+ u" times. It ran "
			+ GeneralUtils::to_u16string(totalRuns)
			+ u" times. Averaging out at "
			+ GeneralUtils::to_u16string((float)totalRuns / loops);

		ChatPackets::SendSystemMessage(sysAddr, message);
	}

	if (chatCommand == "deleteinven" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		eInventoryType inventoryType = eInventoryType::INVALID;
		if (!GeneralUtils::TryParse(args[0], inventoryType)) {
			// In this case, we treat the input as a string and try to find it in the reflection list
			std::transform(args[0].begin(), args[0].end(), args[0].begin(), ::toupper);
			Game::logger->Log("SlashCommandHandler", "looking for inventory %s", args[0].c_str());
			for (uint32_t index = 0; index < NUMBER_OF_INVENTORIES; index++) {
				if (std::string_view(args[0]) == std::string_view(InventoryType::InventoryTypeToString(static_cast<eInventoryType>(index)))) inventoryType = static_cast<eInventoryType>(index);
			}
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
		Game::logger->Log("SlashCommandHandler", "Deleted inventory %s for user %llu", args[0].c_str(), entity->GetObjectID());
		ChatPackets::SendSystemMessage(sysAddr, u"Deleted inventory " + GeneralUtils::UTF8ToUTF16(args[0]));
	}

	if (chatCommand == "inspect" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
		Entity* closest = nullptr;

		eReplicaComponentType component;

		std::u16string ldf;

		bool isLDF = false;

		if (!GeneralUtils::TryParse(args[0], component)) {
			component = eReplicaComponentType::INVALID;

			ldf = GeneralUtils::UTF8ToUTF16(args[0]);

			isLDF = true;
		}

		auto reference = entity->GetPosition();

		auto closestDistance = 0.0f;

		const auto candidates = EntityManager::Instance()->GetEntitiesByComponent(component);

		for (auto* candidate : candidates) {
			if (candidate->GetLOT() == 1 || candidate->GetLOT() == 8092) {
				continue;
			}

			if (isLDF && !candidate->HasVar(ldf)) {
				continue;
			}

			if (closest == nullptr) {
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

		if (closest == nullptr) {
			return;
		}

		EntityManager::Instance()->SerializeEntity(closest);

		auto* table = CDClientManager::Instance()->GetTable<CDObjectsTable>();

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

		if (args.size() >= 2) {
			if (args[1] == "-m" && args.size() >= 3) {
				auto* movingPlatformComponent = closest->GetComponent<MovingPlatformComponent>();

				int32_t value = 0;

				if (movingPlatformComponent == nullptr || !GeneralUtils::TryParse(args[2], value)) {
					return;
				}

				movingPlatformComponent->SetSerialized(true);

				if (value == -1) {
					movingPlatformComponent->StopPathing();
				} else {
					movingPlatformComponent->GotoWaypoint(value);
				}

				EntityManager::Instance()->SerializeEntity(closest);
			} else if (args[1] == "-a" && args.size() >= 3) {
				GameMessages::SendPlayAnimation(closest, GeneralUtils::UTF8ToUTF16(args[2]));
			} else if (args[1] == "-s") {
				for (auto* entry : closest->GetSettings()) {
					ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(entry->GetString()));
				}

				ChatPackets::SendSystemMessage(sysAddr, u"------");
				ChatPackets::SendSystemMessage(sysAddr, u"Spawner ID: " + GeneralUtils::to_u16string(closest->GetSpawnerID()));
			} else if (args[1] == "-p") {
				const auto postion = closest->GetPosition();

				ChatPackets::SendSystemMessage(
					sysAddr,
					GeneralUtils::ASCIIToUTF16("< " + std::to_string(postion.x) + ", " + std::to_string(postion.y) + ", " + std::to_string(postion.z) + " >")
				);
			} else if (args[1] == "-f") {
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

				if (args.size() >= 3) {
					int32_t faction;
					if (!GeneralUtils::TryParse(args[2], faction)) {
						return;
					}

					destuctable->SetFaction(-1);
					destuctable->AddFaction(faction, true);
				}
			} else if (args[1] == "-t") {
				auto* phantomPhysicsComponent = closest->GetComponent<PhantomPhysicsComponent>();

				if (phantomPhysicsComponent != nullptr) {
					ChatPackets::SendSystemMessage(sysAddr, u"Type: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetEffectType())));
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
}

bool SlashCommandHandler::CheckIfAccessibleZone(const unsigned int zoneID) {
	//We're gonna go ahead and presume we've got the db loaded already:
	CDZoneTableTable* zoneTable = CDClientManager::Instance()->GetTable<CDZoneTableTable>();
	const CDZoneTable* zone = zoneTable->Query(zoneID);
	if (zone != nullptr) {
		return Game::assetManager->HasFile(("maps/" + zone->zoneName).c_str());
	} else {
		return false;
	}
}

void SlashCommandHandler::SendAnnouncement(const std::string& title, const std::string& message) {
	AMFArrayValue args;
	auto* titleValue = new AMFStringValue();
	titleValue->SetStringValue(title);
	auto* messageValue = new AMFStringValue();
	messageValue->SetStringValue(message);

	args.InsertValue("title", titleValue);
	args.InsertValue("message", messageValue);

	GameMessages::SendUIMessageServerToAllClients("ToggleAnnounce", &args);

	//Notify chat about it
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ANNOUNCEMENT);

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
