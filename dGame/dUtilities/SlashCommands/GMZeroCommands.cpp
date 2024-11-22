#include "GMZeroCommands.h"

// Classes
#include "Amf3.h"
#include "BinaryPathFinder.h"
#include "ChatPackets.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "Mail.h"
#include "PlayerManager.h"
#include "SlashCommandHandler.h"
#include "VanityUtilities.h"
#include "WorldPackets.h"
#include "ZoneInstanceManager.h"

// Components
#include "BuffComponent.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "ScriptedActivityComponent.h"
#include "SkillComponent.h"

// Emuns
#include "eGameMasterLevel.h"

namespace GMZeroCommands {
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

	//For client side commands
	void ClientHandled(Entity* entity, const SystemAddress& sysAddr, const std::string args) {}

};

