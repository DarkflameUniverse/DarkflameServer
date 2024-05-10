#include "DEVGMCommands.h"

// Classes
#include "AssetManager.h"
#include "Character.h"
#include "ChatPackets.h"
#include "dConfig.h"
#include "dNavMesh.h"
#include "dpWorld.h"
#include "dServer.h"
#include "dpShapeSphere.h"
#include "dZoneManager.h"
#include "EntityInfo.h"
#include "Metrics.hpp"
#include "PlayerManager.h"
#include "SlashCommandHandler.h"
#include "UserManager.h"
#include "User.h"
#include "VanityUtilities.h"
#include "WorldPackets.h"
#include "ZoneInstanceManager.h"

// Database
#include "Database.h"
#include "CDObjectsTable.h"
#include "CDRewardCodesTable.h"

// Components
#include "BuffComponent.h"
#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "InventoryComponent.h"
#include "LevelProgressionComponent.h"
#include "MissionComponent.h"
#include "MovingPlatformComponent.h"
#include "PossessorComponent.h"
#include "ProximityMonitorComponent.h"
#include "RenderComponent.h"
#include "ScriptedActivityComponent.h"
#include "SkillComponent.h"
#include "TriggerComponent.h"
#include "RigidbodyPhantomPhysicsComponent.h"

// Enums
#include "eGameMasterLevel.h"
#include "eMasterMessageType.h"
#include "eInventoryType.h"
#include "ePlayerFlag.h"


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
			std::string message = std::to_string(tables.getIntField("id")) + " - " + tables.getStringField("name");
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
				ChatPackets::SendSystemMessage(sysAddr, u"Failed to spawn entity.");
				return;
			}

			Game::entityManager->ConstructEntity(newEntity);
			numberToSpawn--;
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
		for (const auto* en : entities) {
			const auto* phys = static_cast<PhantomPhysicsComponent*>(en->GetComponent(eReplicaComponentType::PHANTOM_PHYSICS));
			if (phys)
				phys->SpawnVertices();
		}
		for (const auto* en : Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS)) {
			const auto* phys = en->GetComponent<RigidbodyPhantomPhysicsComponent>();
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
