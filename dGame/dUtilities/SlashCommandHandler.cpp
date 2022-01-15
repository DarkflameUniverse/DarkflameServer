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
#include "GameConfig.h"
#include "ScriptedActivityComponent.h"

std::string gen_random(const int len) {
    std::srand(std::time(nullptr));
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

// This custom function will allow us to resolve Character IDs to its logged account.
int ResolveAccountId(int charId) {
    int accid = 0; // Returns 0 if the result was null/empty for the parent function to handle.
    auto grabstmt = Database::CreatePreppedStmt("SELECT account_id FROM charinfo WHERE id = ? LIMIT 1;");
    grabstmt->setInt(1, charId);

    sql::ResultSet* res = grabstmt->executeQuery();
    
	if (res->next()) accid = res->getInt(1);

	delete grabstmt;
    return accid;
}


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

    //Game::logger->Log("SlashCommandHandler", "Received chat command \"%s\"\n", GeneralUtils::UTF16ToWTF8(command).c_str());
    
    User* user = UserManager::Instance()->GetUser(sysAddr);
    if ((chatCommand == "setgmlevel" || chatCommand == "makegm" || chatCommand == "gmlevel") && user->GetMaxGMLevel() > GAME_MASTER_LEVEL_CIVILIAN) {
        if (args.size() != 1) return;
    	
		uint32_t level;

		if (!GeneralUtils::TryParse(args[0], level))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid gm level.");
			return;
		}

#ifndef DEVELOPER_SERVER
		if (user->GetMaxGMLevel() == GAME_MASTER_LEVEL_JUNIOR_DEVELOPER)
		{
			level = GAME_MASTER_LEVEL_CIVILIAN;
		}
#endif

		if (level > user->GetMaxGMLevel())
		{
			level = user->GetMaxGMLevel();
		}

        if (level == entity->GetGMLevel()) return;
        bool success = user->GetMaxGMLevel() >= level;
        
        if (success) {
			if (entity->GetGMLevel() > GAME_MASTER_LEVEL_CIVILIAN && level == GAME_MASTER_LEVEL_CIVILIAN)
			{
				GameMessages::SendToggleGMInvis(entity->GetObjectID(), false, UNASSIGNED_SYSTEM_ADDRESS);
			}
			else if (entity->GetGMLevel() == GAME_MASTER_LEVEL_CIVILIAN && level > GAME_MASTER_LEVEL_CIVILIAN)
			{
				GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
			}

        	WorldPackets::SendGMLevelChange(sysAddr, success, user->GetMaxGMLevel(), entity->GetGMLevel(), level);
            GameMessages::SendChatModeUpdate(entity->GetObjectID(), level);
            entity->SetGMLevel(level);
            Game::logger->Log("SlashCommandHandler", "User %s (%i) has changed their GM level to %i for charID %llu\n", user->GetUsername().c_str(), user->GetAccountID(), level, entity->GetObjectID());
        }
    }

#ifndef DEVELOPER_SERVER
	if ((entity->GetGMLevel() > user->GetMaxGMLevel()) || (entity->GetGMLevel() > GAME_MASTER_LEVEL_CIVILIAN && user->GetMaxGMLevel() == GAME_MASTER_LEVEL_JUNIOR_DEVELOPER))
	{
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
	 
	if (chatCommand == "verify") { // Redi's Custom Command for Verification Interactions
            uint32_t id = ResolveAccountId(entity->GetCharacter()->GetID());

			if (id == 0) {
                ChatPackets::SendSystemMessage(sysAddr, u"There was an issue fetching your account details.");
                return;
			}

            std::string code = "C_" + gen_random(10);
			
			// Check if the verification stage is already done or not.
			// Displays the Discord ID fetched. 0 Means default/not verified.
            std::string discordid; 
            auto grabstmt = Database::CreatePreppedStmt("SELECT verify_id FROM discord_verification WHERE acc_id = ? LIMIT 1;");
            grabstmt->setInt(1, id);

            sql::ResultSet* res = grabstmt->executeQuery();
            
			if (res->rowsCount() != 1) { // This means that the verification records do not exist. (Hasn't ever been executed.)
                discordid = "0";
                auto insertdb = Database::CreatePreppedStmt("INSERT INTO discord_verification (acc_id, verify_id, verify_code) VALUES (?, ?, ?)");
                insertdb->setInt(1, id);
                insertdb->setString(2, "0");
                insertdb->setString(3, code);
                insertdb->execute();
				delete insertdb;
            } else if (res->next()) {
                discordid = res->getString(1);
            }

			delete res;

			if (discordid != "0") {
                ChatPackets::SendSystemMessage(sysAddr, u"Your account has already been verified. If you have any questions, please raise it with a Mythran @ Luplo Discord.");
				return;
			}

			delete grabstmt;

			// Overwrite code with a new one.
			auto updatestmt = Database::CreatePreppedStmt("UPDATE discord_verification SET verify_code = ? WHERE acc_id = ?;");
            updatestmt->setString(1, code);
            updatestmt->setInt(2, id);
            updatestmt->execute();
            delete updatestmt;

            std::stringstream message;
            message << "Your verification code is [" << code << "]. Verify by sending the code to the bot's DMs.";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
            return;
	}

	if (chatCommand == "pvp") {
		auto* character = entity->GetComponent<CharacterComponent>();

		if (character == nullptr) {
			Game::logger->Log("SlashCommandHandler", "Failed to find character component!\n");
			return;
		}

		character->SetPvpEnabled(!character->GetPvpEnabled());
		EntityManager::Instance()->SerializeEntity(entity);
		
		std::stringstream message;
		message << character->GetName() << " changed their PVP flag to " << std::to_string(character->GetPvpEnabled()) << "!";
		
		ChatPackets::SendSystemMessage(UNASSIGNED_SYSTEM_ADDRESS, GeneralUtils::ASCIIToUTF16(message.str()), true);

		return;
	}

	if (chatCommand == "who")
	{
		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Players in this instance: (" + GeneralUtils::to_u16string(Player::GetAllPlayers().size()) + u")"
		);

		for (auto* player : Player::GetAllPlayers())
		{
			const auto& name = player->GetCharacter()->GetName();

			ChatPackets::SendSystemMessage(
				sysAddr,
				GeneralUtils::ASCIIToUTF16(player == entity ? name + " (you)" : name)
			);
		}
	}

	if (chatCommand == "ping") {
		if (!args.empty() && args[0] == "-l")
		{
			std::stringstream message;
			message << "Your latest ping: " << std::to_string(Game::server->GetLatestPing(sysAddr)) << "ms";
			
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		}
		else
		{
			std::stringstream message;
			message << "Your average ping: " << std::to_string(Game::server->GetPing(sysAddr)) << "ms";
			
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message.str()));
		}
		return;
	}

	if (chatCommand == "skip-ags")
	{
		auto* missionComponent = entity->GetComponent<MissionComponent>();

		if (missionComponent != nullptr && missionComponent->HasMission(479))
		{
			missionComponent->CompleteMission(479);
		}
	}

	if (chatCommand == "skip-sg")
	{
		auto* missionComponent = entity->GetComponent<MissionComponent>();

		if (missionComponent != nullptr && missionComponent->HasMission(229))
		{
			missionComponent->CompleteMission(229);
		}
	}

	if (chatCommand == "fix-stats")
	{
		// Reset skill component and buff component
		auto* skillComponent = entity->GetComponent<SkillComponent>();
		auto* buffComponent = entity->GetComponent<BuffComponent>();
		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		// If any of the components are nullptr, return
		if (skillComponent == nullptr || buffComponent == nullptr || destroyableComponent == nullptr)
		{
			return;
		}

		// Reset skill component
		skillComponent->Reset();

		// Reset buff component
		buffComponent->Reset();

		// Fix the destroyable component
		destroyableComponent->FixStats();
	}

	if (chatCommand == "credits" || chatCommand == "info")
	{
		const auto& customText = chatCommand == "credits" ? VanityUtilities::ParseMarkdown("./vanity/CREDITS.md") : VanityUtilities::ParseMarkdown("./vanity/INFO.md");

		{
			AMFArrayValue args;

			auto* state = new AMFStringValue();
			state->SetStringValue("Story");

			args.InsertValue("state", state);

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "pushGameState", &args);

			delete state;
		}

		entity->AddCallbackTimer(0.5f, [customText, entity] ()
		{
			AMFArrayValue args;

			auto* visiable = new AMFTrueValue();
			auto* text = new AMFStringValue();
			text->SetStringValue(customText);

			args.InsertValue("visible", visiable);
			args.InsertValue("text", text);

			Game::logger->Log("SlashCommandHandler", "Sending \n%s\n", customText.c_str());

			GameMessages::SendUIMessageServerToSingleClient(entity, entity->GetSystemAddress(), "ToggleStoryBox", &args);
			
			delete visiable;
			delete text;
		});

		return;
	}

	if ((chatCommand == "leave-zone")) {
		const auto currentZone = dZoneManager::Instance()->GetZone()->GetZoneID().GetMapID();

		auto newZone = 1100;

		switch (currentZone)
		{
		case 1101:
			newZone = 1100;
			break;
		case 1204:
			newZone = 1200;
			break;
		default:
			newZone = 1100;
			break;
		}

		if (currentZone == newZone)
		{
			ChatPackets::SendSystemMessage(sysAddr, u"You are not in an instanced zone.");
			
			return;
		}
		
		ChatPackets::SendSystemMessage(sysAddr, u"Leaving zone...");

		const auto objid = entity->GetObjectID();

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, newZone, 0, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort)
			{
				auto* entity = EntityManager::Instance()->GetEntity(objid);

				if (entity == nullptr) {
					return;
				}

				const auto sysAddr = entity->GetSystemAddress();

				Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i\n", entity->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

				if (entity->GetCharacter()) {
					entity->GetCharacter()->SetZoneID(zoneID);
					entity->GetCharacter()->SetZoneInstance(zoneInstance);
					entity->GetCharacter()->SetZoneClone(zoneClone);
				}

				entity->GetCharacter()->SaveXMLToDatabase();

				WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
			});
	}

	if ((chatCommand == "join" && !args.empty())) {
		ChatPackets::SendSystemMessage(sysAddr, u"Requesting private map...");
		const auto& password = args[0];
		
		ZoneInstanceManager::Instance()->RequestPrivateZone(Game::server, false, password, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort)
			{
				Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i\n", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

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
		if ((chatCommand == "playanimation" || chatCommand == "playanim") && args.size() == 1) {
			std::u16string anim = GeneralUtils::ASCIIToUTF16(args[0], args[0].size());
			GameMessages::SendPlayAnimation(entity, anim);
		}

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
	
	if (chatCommand == "setMinifig" && args.size() == 2 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_FORUM_MODERATOR) { // could break characters so only allow if GM > 0
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
		} else if (lowerName == "shirt") {
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

	if (chatCommand == "list-spawns" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		for (const auto& pair : EntityManager::Instance()->GetSpawnPointEntities()) {
			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(pair.first));
		}
		
		ChatPackets::SendSystemMessage(sysAddr, u"Current: " + GeneralUtils::ASCIIToUTF16(entity->GetCharacter()->GetTargetScene()));

		return;
	}
	
	if (chatCommand == "unlock-emote" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t emoteID;
		
		if (!GeneralUtils::TryParse(args[0], emoteID))
		{
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

	if (chatCommand == "speedboost" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		float boost;
		
		if (!GeneralUtils::TryParse(args[0], boost))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid boost.");
			return;
		}

		auto* controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();

		if (controllablePhysicsComponent == nullptr)
		{
			return;
		}

		controllablePhysicsComponent->SetSpeedMultiplier(boost);

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

		if (!GeneralUtils::TryParse(args[0], scheme))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid control scheme.");
			return;
		}
		
		GameMessages::SendSetPlayerControlScheme(entity, static_cast<eControlSceme>(scheme));
		
		ChatPackets::SendSystemMessage(sysAddr, u"Switched control scheme.");
		return;
	}

	if (chatCommand == "approveproperty" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_LEAD_MODERATOR) {

		if (PropertyManagementComponent::Instance() != nullptr)
		{
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

		delete value;

		return;
	}

	if (chatCommand == "toggle" && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		AMFTrueValue* value = new AMFTrueValue();

		AMFArrayValue amfArgs;
		amfArgs.InsertValue("visible", value);
		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, args[0], &amfArgs);
		
		ChatPackets::SendSystemMessage(sysAddr, u"Toggled UI state.");

		delete value;

		return;
	}

	if ((chatCommand == "setinventorysize" || chatCommand == "setinvsize") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() != 1) return;

		uint32_t size;

		if (!GeneralUtils::TryParse(args[0], size))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid size.");
			return;
		}
		
		InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(COMPONENT_TYPE_INVENTORY));
		if (inventory)
		{
			auto* items = inventory->GetInventory(ITEMS);
			
			items->SetSize(size);
		}

		return;
	}

	if (chatCommand == "runmacro" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() != 1) return;

		std::ifstream infile("./res/macros/" + args[0] + ".scm");

		if (infile.good()) {
			std::string line;
			while (std::getline(infile, line)) {
				SlashCommandHandler::HandleChatCommand(GeneralUtils::ASCIIToUTF16(line), entity, sysAddr);
			}
		}
		else {
			ChatPackets::SendSystemMessage(sysAddr, u"Unknown macro! Is the filename right?");
		}
		
		return;
	}

	if (chatCommand == "addmission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}
		
		auto comp = static_cast<MissionComponent*>(entity->GetComponent(COMPONENT_TYPE_MISSION));
		if (comp) comp->AcceptMission(missionID, true);
		return;
	}

	if (chatCommand == "completemission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}
		
		auto comp = static_cast<MissionComponent*>(entity->GetComponent(COMPONENT_TYPE_MISSION));
		if (comp) comp->CompleteMission(missionID, true);
		return;
	}

	if (chatCommand == "setflag" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1)
	{
		uint32_t flagId;

		if (!GeneralUtils::TryParse(args[0], flagId))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}

        entity->GetCharacter()->SetPlayerFlag(flagId, true);
	}

	if (chatCommand == "clearflag" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1)
	{
		uint32_t flagId;

		if (!GeneralUtils::TryParse(args[0], flagId))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid flag id.");
			return;
		}

        entity->GetCharacter()->SetPlayerFlag(flagId, false);
	}

	if (chatCommand == "resetmission" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		if (args.size() == 0) return;

		uint32_t missionID;

		if (!GeneralUtils::TryParse(args[0], missionID))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid mission id.");
			return;
		}
		
		auto* comp = static_cast<MissionComponent*>(entity->GetComponent(COMPONENT_TYPE_MISSION));

		if (comp == nullptr) {
			return;
		}

		auto* mission = comp->GetMission(missionID);

		if (mission == nullptr) {
			return;
		}
		
		mission->SetMissionState(MissionState::MISSION_STATE_ACTIVE);

		return;
	}

	if (chatCommand == "playeffect" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 3) {
		int32_t effectID = 0;

		if (!GeneralUtils::TryParse(args[0], effectID)) {
			return;
		}

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
		auto control = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS));
		if (!control) return;

		float y = dpWorld::Instance().GetHeightAtPoint(control->GetPosition());
		std::u16string msg = u"Navmesh height: " + (GeneralUtils::to_u16string(y));
		ChatPackets::SendSystemMessage(sysAddr, msg);
	}

    if (chatCommand == "gmadditem" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
        if (args.size() == 1) {
			uint32_t itemLOT;

			if (!GeneralUtils::TryParse(args[0], itemLOT))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}
			
			InventoryComponent * inventory = static_cast<InventoryComponent*>(entity->GetComponent(COMPONENT_TYPE_INVENTORY));

			inventory->AddItem(itemLOT, 1);
        } else if(args.size() == 2) {
			uint32_t itemLOT;

			if (!GeneralUtils::TryParse(args[0], itemLOT))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
				return;
			}

			uint32_t count;

			if (!GeneralUtils::TryParse(args[1], count))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid item count.");
				return;
			}

			InventoryComponent* inventory = static_cast<InventoryComponent*>(entity->GetComponent(COMPONENT_TYPE_INVENTORY));

			inventory->AddItem(itemLOT, count);
		}
		else {
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

		if (receiverID == 0)
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Failed to find that player");
			
			return;
		}

		uint32_t lot;

		if (!GeneralUtils::TryParse(args[1], lot))
		{
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

	if (chatCommand == "setname" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		std::string name = "";

		for (const auto& arg : args)
		{
			name += arg + " ";
		}

		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
	}
    
	if (chatCommand == "title" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		std::string name = entity->GetCharacter()->GetName() + " - ";

		for (const auto& arg : args)
		{
			name += arg + " ";
		}

		GameMessages::SendSetName(entity->GetObjectID(), GeneralUtils::ASCIIToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);
	}

    if ((chatCommand == "teleport" || chatCommand == "tele") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_MODERATOR) {
        NiPoint3 pos {};
        if (args.size() == 3) {

			float x, y, z;
        	
			if (!GeneralUtils::TryParse(args[0], x))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}
        	
			if (!GeneralUtils::TryParse(args[1], y))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid y.");
				return;
			}
        	
			if (!GeneralUtils::TryParse(args[2], z))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

            pos.SetX(x);
            pos.SetY(y);
            pos.SetZ(z);
            
            Game::logger->Log("SlashCommandHandler", "Teleporting objectID: %llu to %f, %f, %f\n", entity->GetObjectID(), pos.x, pos.y, pos.z);
            GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
        } else if (args.size() == 2) {
			float x, z;

			if (!GeneralUtils::TryParse(args[0], x))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid x.");
				return;
			}

			if (!GeneralUtils::TryParse(args[1], z))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid z.");
				return;
			}

			pos.SetX(x);
			pos.SetY(0.0f);
			pos.SetZ(z);

            
            Game::logger->Log("SlashCommandHandler", "Teleporting objectID: %llu to X: %f, Z: %f\n", entity->GetObjectID(), pos.x, pos.z);
            GameMessages::SendTeleport(entity->GetObjectID(), pos, NiQuaternion(), sysAddr);
        } else {
            ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /teleport <x> (<y>) <z> - if no Y given, will teleport to the height of the terrain (or any physics object).");
        }

		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent != nullptr)
		{
			auto* possassableEntity = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

			if (possassableEntity != nullptr)
			{
				auto* vehiclePhysicsComponent = possassableEntity->GetComponent<VehiclePhysicsComponent>();

				if (vehiclePhysicsComponent != nullptr)
				{
					vehiclePhysicsComponent->SetPosition(pos);

					EntityManager::Instance()->SerializeEntity(possassableEntity);

					Game::logger->Log("ClientPackets", "Forced updated vehicle position\n");
				}
			}
		}
    }

	if (chatCommand == "tpall" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		const auto pos = entity->GetPosition();

		const auto characters = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_CHARACTER);

		for (auto* character : characters)
		{
			GameMessages::SendTeleport(character->GetObjectID(), pos, NiQuaternion(), character->GetSystemAddress());
		}

		return;
	}

	if (chatCommand == "dismount" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		PossessorComponent* possessorComponent;
		if (entity->TryGetComponent(COMPONENT_TYPE_POSSESSOR, possessorComponent)) {
			Entity* vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());
			if (!vehicle) return;

			PossessableComponent* possessableComponent;
			if (vehicle->TryGetComponent(COMPONENT_TYPE_POSSESSABLE, possessableComponent)) {
				possessableComponent->SetPossessor(LWOOBJID_EMPTY);
				possessorComponent->SetPossessable(LWOOBJID_EMPTY);

				EntityManager::Instance()->SerializeEntity(vehicle);
				EntityManager::Instance()->SerializeEntity(entity);
			}
		}
	}

	//------- GM COMMANDS TO ACTUALLY MODERATE --------

	if (chatCommand == "mute" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_JUNIOR_DEVELOPER) {
		if (args.size() >= 1) {
			auto* player = Player::GetPlayer(args[0]);

			uint32_t accountId = 0;
			LWOOBJID characterId = 0;

			if (player == nullptr)
			{
				auto* accountQuery = Database::CreatePreppedStmt("SELECT account_id, id FROM charinfo WHERE name=? LIMIT 1;");
				
				accountQuery->setString(1, args[0]);

				auto result = accountQuery->executeQuery();

				if (result->rowsCount() > 0)
				{
					while (result->next()) {
						accountId = result->getUInt(1);
						characterId = result->getUInt64(2);
						
						characterId = GeneralUtils::SetBit(characterId, OBJECT_BIT_CHARACTER);
						characterId = GeneralUtils::SetBit(characterId, OBJECT_BIT_PERSISTENT);
					}
				}

				delete accountQuery;
				delete result;

				if (accountId == 0)
				{
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::ASCIIToUTF16(args[0]));

					return;
				}
			}
			else
			{
				accountId = player->GetParentUser()->GetAccountID();
				characterId = player->GetCharacter()->GetID();
			}

			auto* userUpdate = Database::CreatePreppedStmt("UPDATE accounts SET mute_expire = ? WHERE id = ?;");

			time_t expire = 1; // Default to indefinate mute

			if (args.size() >= 2)
			{
				uint32_t days = 0;
				uint32_t hours = 0;
				if (!GeneralUtils::TryParse(args[1], days))
				{
					ChatPackets::SendSystemMessage(sysAddr, u"Invalid days.");

					return;
				}

				if (args.size() >= 3)
				{
					if (!GeneralUtils::TryParse(args[2], hours))
					{
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

			if (expire != 1)
			{
				std::tm * ptm = std::localtime(&expire);
				// Format: Mo, 15.06.2009 20:20:00
				std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
			}
			
			const auto timeStr = GeneralUtils::ASCIIToUTF16(std::string(buffer));

			ChatPackets::SendSystemMessage(sysAddr, u"Muted: " + GeneralUtils::ASCIIToUTF16(args[0]) + u" until " + timeStr);

			//Notify chat about it 
			CBITSTREAM;
			PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_MUTE_UPDATE);

			bitStream.Write(characterId);
			bitStream.Write(expire);

			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		}
		else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /mute <username> <days (optional)> <hours (optional)>");
		}
	}

	if (chatCommand == "gamekick" && entity->GetGMLevel() == 1) {
		if (args.size() == 1) {
			auto* player = Player::GetPlayer(args[0]);

			if (player == nullptr)
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::ASCIIToUTF16(args[0]));

				return;
			}

			Game::server->Disconnect(player->GetSystemAddress(), SERVER_DISCON_KICK);

			ChatPackets::SendSystemMessage(sysAddr, u"Kicked: " + GeneralUtils::ASCIIToUTF16(args[0]));
		}
		else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /kick <username>");
		}
	}

	if (chatCommand == "ban" && entity->GetGMLevel() == 1) {
		if (args.size() == 1) {
			auto* player = Player::GetPlayer(args[0]);

			uint32_t accountId = 0;

			if (player == nullptr)
			{
				auto* accountQuery = Database::CreatePreppedStmt("SELECT account_id FROM charinfo WHERE name=? LIMIT 1;");
				
				accountQuery->setString(1, args[0]);

				auto result = accountQuery->executeQuery();

				if (result->rowsCount() > 0)
				{
					while (result->next()) accountId = result->getUInt(1);
				}

				delete accountQuery;
				delete result;

				if (accountId == 0)
				{
					ChatPackets::SendSystemMessage(sysAddr, u"Count not find player of name: " + GeneralUtils::ASCIIToUTF16(args[0]));

					return;
				}
			}
			else
			{
				accountId = player->GetParentUser()->GetAccountID();
			}

			auto* userUpdate = Database::CreatePreppedStmt("UPDATE accounts SET banned = true WHERE id = ?;");

			userUpdate->setInt(1, accountId);

			userUpdate->executeUpdate();

			delete userUpdate;

			if (player != nullptr)
			{
				Game::server->Disconnect(player->GetSystemAddress(), SERVER_DISCON_KICK);
			}

			ChatPackets::SendSystemMessage(sysAddr, u"Banned: " + GeneralUtils::ASCIIToUTF16(args[0]));
		}
		else {
			ChatPackets::SendSystemMessage(sysAddr, u"Correct usage: /ban <username>");
		}
	}

	//-------------------------------------------------
    
    if (chatCommand == "buffme" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(COMPONENT_TYPE_DESTROYABLE));
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
		
		if (!GeneralUtils::TryParse(args[0], celebration))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid celebration.");
			return;
		}

		GameMessages::SendStartCelebrationEffect(entity, entity->GetSystemAddress(), celebration);
	}
    
    if (chatCommand == "buffmed" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(COMPONENT_TYPE_DESTROYABLE));
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
		auto dest = static_cast<DestroyableComponent*>(entity->GetComponent(COMPONENT_TYPE_DESTROYABLE));
		if (dest) {
			dest->SetHealth((int)dest->GetMaxHealth());
			dest->SetArmor((int)dest->GetMaxArmor());
			dest->SetImagination((int)dest->GetMaxImagination());
		}

        EntityManager::Instance()->SerializeEntity(entity);
    }
    
    if (chatCommand == "lookup" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() == 1) {
        std::string query = "SELECT `id`, `name` FROM `Objects` WHERE `displayName` LIKE '%" + args[0] + "%' OR `name` LIKE '%" + args[0] + "%' OR `description` LIKE '%" + args[0] + "%'";
        auto tables = CDClientDatabase::ExecuteQuery(query.c_str());
        while (!tables.eof()) {
            std::string message = std::to_string(tables.getIntField(0)) + " - " + tables.getStringField(1);
            ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message, message.size()));
            tables.nextRow();
        }
    }
    
    if (chatCommand == "spawn" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1) {
        ControllablePhysicsComponent* comp = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS));
        if (!comp) return;

		uint32_t lot;
    	
		if (!GeneralUtils::TryParse(args[0], lot))
		{
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

        EntityManager::Instance()->ConstructEntity(newEntity);
    }

	if ((chatCommand == "giveuscore") && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t uscore;

		if (!GeneralUtils::TryParse(args[0], uscore))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid uscore.");
			return;
		}

		CharacterComponent* character = entity->GetComponent<CharacterComponent>();
		if (character) character->SetUScore(character->GetUScore() + uscore);

		GameMessages::SendModifyLEGOScore(entity, entity->GetSystemAddress(), uscore, LOOTTYPE_NONE);
	}

	if (chatCommand == "pos" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto position = entity->GetPosition();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(position.x)) + u", " +  (GeneralUtils::to_u16string(position.y)) + u", " +  (GeneralUtils::to_u16string(position.z)) +  u">");
	
		std::cout << position.x << ", " << position.y << ", " << position.z << std::endl;
	}

	if (chatCommand == "rot" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		const auto rotation = entity->GetRotation();

		ChatPackets::SendSystemMessage(sysAddr, u"<" + (GeneralUtils::to_u16string(rotation.w)) + u", " +  (GeneralUtils::to_u16string(rotation.x)) + u", " +  (GeneralUtils::to_u16string(rotation.y)) + u", " +  (GeneralUtils::to_u16string(rotation.z)) +  u">");

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

		if (!GeneralUtils::TryParse(args[0], money))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(ch->GetCoins() + money, LOOT_SOURCE_MODERATION);
	}

	if ((chatCommand == "setcurrency") && args.size() == 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		int32_t money;

		if (!GeneralUtils::TryParse(args[0], money))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid money.");
			return;
		}

		auto* ch = entity->GetCharacter();
		ch->SetCoins(money, LOOT_SOURCE_MODERATION);
	}

	// Allow for this on even while not a GM, as it sometimes toggles incorrrectly.
	if (chatCommand == "gminvis" && entity->GetParentUser()->GetMaxGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, UNASSIGNED_SYSTEM_ADDRESS);
		
		return;
	}

	if (chatCommand == "gmimmune" && args.size() >= 1 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		int32_t state = false;

		if (!GeneralUtils::TryParse(args[0], state))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid state.");
			return;
		}

		if (destroyableComponent != nullptr)
		{
			destroyableComponent->SetIsGMImmune(state);
		}
		
		return;
	}

	if (chatCommand == "buff" && args.size() >= 2 && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		auto* buffComponent = entity->GetComponent<BuffComponent>();

		int32_t id = 0;
		int32_t duration = 0;

		if (!GeneralUtils::TryParse(args[0], id))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff id.");
			return;
		}

		if (!GeneralUtils::TryParse(args[1], duration))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid buff duration.");
			return;
		}

		if (buffComponent != nullptr)
		{
			buffComponent->ApplyBuff(id, duration, entity->GetObjectID());
		}
		
		return;
	}

    if ((chatCommand == "tomap" && args.size() >= 1) && entity->GetGMLevel() >= GAME_MASTER_LEVEL_FORUM_MODERATOR) {
        ChatPackets::SendSystemMessage(sysAddr, u"Requesting map change...");
		uint32_t reqZone;
		LWOCLONEID cloneId = 0;
		bool force = false;

		if (!GeneralUtils::TryParse(args[0], reqZone))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}

		if (args.size() > 1)
		{
			auto index = 1;

			if (args[index] == "force")
			{
				index++;

				force = true;
			}

			if (args.size() > index && !GeneralUtils::TryParse(args[index], cloneId))
			{
				ChatPackets::SendSystemMessage(sysAddr, u"Invalid clone id.");
				return;
			}
		}

		const auto objid = entity->GetObjectID();
    	
        if (force || CheckIfAccessibleZone(reqZone)) { // to prevent tomfoolery
			bool darwin = true; //Putting this on true, as I'm sick of having to wait 3-4 seconds on a transfer while trying to quickly moderate properties
			
			Character* character = entity->GetCharacter();
			if (character) {
				std::string lowerName = character->GetName();
				std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
				// feel free to add your name to the list
				if (lowerName.find("max") != std::string::npos || lowerName.find("darwin") != std::string::npos || lowerName.find("gie") != std::string::npos) {
					darwin = true;
				}
			}

			if (!darwin) {
				GameMessages::SendPlayAnimation(entity, u"lup-teleport");
				GameMessages::SendSetStunned(objid, PUSH, user->GetSystemAddress(),
					LWOOBJID_EMPTY, true, true, true, true, true, true, true, true
				);
			}

            ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, reqZone, cloneId, false, [objid, darwin](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
				auto* entity = EntityManager::Instance()->GetEntity(objid);

            	if (entity == nullptr) {
            		return;
            	}

				float transferTime = 3.32999992370605f;
				if (darwin) transferTime = 0.0f;

				entity->AddCallbackTimer(transferTime, [=] {
					const auto sysAddr = entity->GetSystemAddress();

					ChatPackets::SendSystemMessage(sysAddr, u"Transfering map...");

					Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i\n", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
					if (entity->GetCharacter()) {
						entity->GetCharacter()->SetZoneID(zoneID);
						entity->GetCharacter()->SetZoneInstance(zoneInstance);
						entity->GetCharacter()->SetZoneClone(zoneClone);
						entity->GetComponent<CharacterComponent>()->SetLastRocketConfig(u"");
					}

					entity->GetCharacter()->SaveXMLToDatabase();

					WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
				});
				
                return;
            });
        } else {
            std::string msg = "ZoneID not found or allowed: ";
            msg.append(args[0]);
            ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(msg, msg.size()));
        }
    }

	if (chatCommand == "createprivate" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 3)
	{
		uint32_t zone;

		if (!GeneralUtils::TryParse(args[0], zone))
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Invalid zone.");
			return;
		}

		uint32_t clone;

		if (!GeneralUtils::TryParse(args[1], clone))
		{
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
		AMFStringValue* value = new AMFStringValue();
		value->SetStringValue("ToggleUIDebugger;");
		AMFArrayValue args;
		GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, value->GetStringValue(), &args);
		delete value;
	}

	if ((chatCommand == "boost") && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr)
		{
			return;
		}

		auto* vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr)
		{
			return;
		}

		GameMessages::SendVehicleAddPassiveBoostAction(vehicle->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
	}

	if (chatCommand == "activatespawner" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1)
	{
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(args[0]);

		for (auto* spawner : spawners)
		{
			spawner->Activate();
		}

		spawners = dZoneManager::Instance()->GetSpawnersInGroup(args[0]);

		for (auto* spawner : spawners)
		{
			spawner->Activate();
		}
	}

	if (chatCommand == "spawnphysicsverts" && entity->GetGMLevel() >= 6) {
		//Go tell physics to spawn all the vertices:
		auto entities = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_PHANTOM_PHYSICS);
		for (auto en : entities) {
			auto phys = static_cast<PhantomPhysicsComponent*>(en->GetComponent(COMPONENT_TYPE_PHANTOM_PHYSICS));
			if (phys)
				phys->SpawnVertices();
		}
	}

	if (chatCommand == "reportproxphys" && entity->GetGMLevel() >= 6) {
		auto entities = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_PROXIMITY_MONITOR);
		for (auto en : entities) {
			auto phys = static_cast<ProximityMonitorComponent*>(en->GetComponent(COMPONENT_TYPE_PROXIMITY_MONITOR));
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
	
	if (chatCommand == "triggerspawner" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1)
	{
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(args[0]);

		for (auto* spawner : spawners)
		{
			spawner->Spawn();
		}

		spawners = dZoneManager::Instance()->GetSpawnersInGroup(args[0]);

		for (auto* spawner : spawners)
		{
			spawner->Spawn();
		}
	}

	if (chatCommand == "reforge" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 2)
	{
		LOT baseItem;
		LOT reforgedItem;

		if (!GeneralUtils::TryParse(args[0], baseItem)) return;
		if (!GeneralUtils::TryParse(args[1], reforgedItem)) return;

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (inventoryComponent == nullptr) return;

		std::vector<LDFBaseData*> data {};
		data.push_back(new LDFData<int32_t>(u"reforgedLOT", reforgedItem));

		inventoryComponent->AddItem(baseItem, 1, INVALID, data);
	}

	if (chatCommand == "crash" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_OPERATOR)
	{
		ChatPackets::SendSystemMessage(sysAddr, u"Crashing...");

		int* badPtr = nullptr;
		*badPtr = 0;

		return;
	}

	if (chatCommand == "config-set" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 2)
	{
		GameConfig::SetValue(args[0], args[1]);

		ChatPackets::SendSystemMessage(
			sysAddr, u"Set config value: " + GeneralUtils::ASCIIToUTF16(args[0]) + u" to " + GeneralUtils::ASCIIToUTF16(args[1])
		);
	}

	if (chatCommand == "config-get" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1)
	{
		const auto& value = GameConfig::GetValue(args[0]);

		if (value.empty())
		{
			ChatPackets::SendSystemMessage(sysAddr, u"No value found for " + GeneralUtils::ASCIIToUTF16(args[0]));
		}
		else
		{
			ChatPackets::SendSystemMessage(sysAddr, u"Value for " + GeneralUtils::ASCIIToUTF16(args[0]) + u": " + GeneralUtils::ASCIIToUTF16(value));
		}
	}

	if (chatCommand == "metrics" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER)
	{
		for (const auto variable : Metrics::GetAllMetrics())
		{
			auto* metric = Metrics::GetMetric(variable);

			if (metric == nullptr)
			{
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
			u"Peak RSS: " + GeneralUtils::to_u16string((float) ((double) Metrics::GetPeakRSS() / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Current RSS: " + GeneralUtils::to_u16string((float) ((double) Metrics::GetCurrentRSS() / 1.024e6)) +
			u"MB"
		);

		ChatPackets::SendSystemMessage(
			sysAddr,
			u"Process ID: " + GeneralUtils::to_u16string(Metrics::GetProcessID())
		);

		return;
	}

	if (chatCommand == "inspect" && entity->GetGMLevel() >= GAME_MASTER_LEVEL_DEVELOPER && args.size() >= 1)
	{
		Entity* closest = nullptr;

		int32_t component;

		std::u16string ldf;

		bool isLDF = false;

		if (!GeneralUtils::TryParse(args[0], component))
		{
			component = -1;

			ldf = GeneralUtils::ASCIIToUTF16(args[0]);

			isLDF = true;
		}

		auto reference = entity->GetPosition();

		auto closestDistance = 0.0f;

		const auto candidates = EntityManager::Instance()->GetEntitiesByComponent(component);
		
		for (auto* candidate : candidates)
		{
			if (candidate->GetLOT() == 1 || candidate->GetLOT() == 8092)
			{
				continue;
			}

			if (isLDF && !candidate->HasVar(ldf))
			{
				continue;
			}
			
			if (closest == nullptr)
			{
				closest = candidate;

				closestDistance = NiPoint3::Distance(candidate->GetPosition(), reference);
				
				continue;
			}
			
			const auto distance = NiPoint3::Distance(candidate->GetPosition(), reference);

			if (distance < closestDistance)
			{
				closest = candidate;

				closestDistance = distance;
			}
		}

		if (closest == nullptr)
		{
			return;
		}

		EntityManager::Instance()->SerializeEntity(closest);

		auto* table = CDClientManager::Instance()->GetTable<CDObjectsTable>("Objects");

		const auto& info = table->GetByID(closest->GetLOT());

		std::stringstream header;

		header << info.name << " [" << std::to_string(info.id) << "]" << " " << std::to_string(closestDistance) << " " << std::to_string(closest->IsSleeping());

		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(header.str()));
		
		for (const auto& pair : closest->GetComponents())
		{
			auto id = pair.first;

			std::stringstream stream;

			stream << "Component [" << std::to_string(id) << "]";

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(stream.str()));
		}

		if (args.size() >= 2)
		{	
			if (args[1] == "-m" && args.size() >= 3)
			{
				auto* movingPlatformComponent = closest->GetComponent<MovingPlatformComponent>();

				int32_t value = 0;

				if (movingPlatformComponent == nullptr || !GeneralUtils::TryParse(args[2], value))
				{
					return;
				}

				movingPlatformComponent->SetSerialized(true);

				if (value == -1)
				{
					movingPlatformComponent->StopPathing();
				}
				else
				{
					movingPlatformComponent->GotoWaypoint(value);
				}

				EntityManager::Instance()->SerializeEntity(closest);
			}
			else if (args[1] == "-a" && args.size() >= 3)
			{
				GameMessages::SendPlayAnimation(closest, GeneralUtils::ASCIIToUTF16(args[2]));
			}
			else if (args[1] == "-s")
			{
				for (auto* entry : closest->GetSettings())
				{
					ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(entry->GetString()));
				}

				ChatPackets::SendSystemMessage(sysAddr, u"------");
				ChatPackets::SendSystemMessage(sysAddr, u"Spawner ID: " + GeneralUtils::to_u16string(closest->GetSpawnerID()));
			}
			else if (args[1] == "-p")
			{
				const auto postion = closest->GetPosition();

				ChatPackets::SendSystemMessage(
					sysAddr, 
					GeneralUtils::ASCIIToUTF16("< " + std::to_string(postion.x) + ", " + std::to_string(postion.y) + ", " + std::to_string(postion.z) + " >")
				);
			}
			else if (args[1] == "-f")
			{
				auto* destuctable = closest->GetComponent<DestroyableComponent>();

				if (destuctable == nullptr)
				{
					ChatPackets::SendSystemMessage(sysAddr, u"No destroyable component on this entity!");
					return;
				}

				ChatPackets::SendSystemMessage(sysAddr, u"Smashable: " + (GeneralUtils::to_u16string(destuctable->GetIsSmashable())));

				ChatPackets::SendSystemMessage(sysAddr, u"Friendly factions:");
				for (const auto entry : destuctable->GetFactionIDs())
				{
					ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
				}

				ChatPackets::SendSystemMessage(sysAddr, u"Enemy factions:");
				for (const auto entry : destuctable->GetEnemyFactionsIDs())
				{
					ChatPackets::SendSystemMessage(sysAddr, (GeneralUtils::to_u16string(entry)));
				}

				if (args.size() >= 3)
				{
					int32_t faction;
					if (!GeneralUtils::TryParse(args[2], faction))
					{
						return;
					}

					destuctable->SetFaction(-1);
					destuctable->AddFaction(faction, true);
				}
			}
			else if (args[1] == "-t")
			{
				auto* phantomPhysicsComponent = closest->GetComponent<PhantomPhysicsComponent>();

				if (phantomPhysicsComponent != nullptr)
				{
					ChatPackets::SendSystemMessage(sysAddr, u"Type: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetEffectType())));
					const auto dir = phantomPhysicsComponent->GetDirection();
					ChatPackets::SendSystemMessage(sysAddr, u"Direction: <" + (GeneralUtils::to_u16string(dir.x)) + u", " + (GeneralUtils::to_u16string(dir.y)) + u", "+ (GeneralUtils::to_u16string(dir.z)) + u">");
					ChatPackets::SendSystemMessage(sysAddr, u"Multiplier: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetDirectionalMultiplier())));
					ChatPackets::SendSystemMessage(sysAddr, u"Active: " + (GeneralUtils::to_u16string(phantomPhysicsComponent->GetPhysicsEffectActive())));
				}

				if (closest->GetTrigger() != nullptr)
				{
					ChatPackets::SendSystemMessage(sysAddr, u"Trigger: " + (GeneralUtils::to_u16string(closest->GetTrigger()->id)));
				}
			}
		}
	}
}

bool SlashCommandHandler::CheckIfAccessibleZone(const unsigned int zoneID) {
    switch (zoneID) {
	case 98:
        case 1000:
        case 1001:
            
        case 1100:
        case 1101:
	case 1150:
	case 1151:
	case 1152:
            
        case 1200:
        case 1201:

	case 1250:
	case 1251:
	case 1260:
            
        case 1300:
    	case 1350:
    	case 1351:
		    
        case 1400:
	case 1401:
	case 1450:
	case 1451:
            
        case 1600:
        case 1601:
        case 1602:
        case 1603:
        case 1604:
            
        case 1800:
        case 1900:
        case 2000:

	case 58004:
	case 58005:
	case 58006:
            return true;
        
        default:
            return false;
    }
    
    return false;
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

	delete titleValue;
	delete messageValue;
	titleValue = nullptr;
	messageValue = nullptr;

	//Notify chat about it 
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ANNOUNCEMENT);

	RakNet::RakString rsTitle(title.c_str());
	RakNet::RakString rsMsg(message.c_str());

	bitStream.Write(rsTitle);
	bitStream.Write(rsMsg);

	Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
}
