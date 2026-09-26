#include "DashboardActions.h"

#include "PlayerAction.h"
#include "Game.h"
#include "Logger.h"
#include "Database.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "Entity.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "GameMessages.h"
#include "PlayerManager.h"
#include "User.h"
#include "UserManager.h"
#include "WorldPackets.h"
#include "ZoneInstanceManager.h"
#include "eGameMasterLevel.h"
#include "eServerDisconnectIdentifiers.h"
#include "InventoryComponent.h"
#include "Amf3.h"
#include "PetComponent.h"
#include "PropertyManagementComponent.h"
#include "GeneralUtils.h"
#include "VanityUtilities.h"
#include "dConfig.h"
#include "dChatFilter.h"
#include "MissionComponent.h"
#include "Mission.h"
#include "LiveEvents.h"

namespace {
	std::function<void(const SystemAddress&)> g_LogoutHandler;

	uint32_t KickAccount(uint32_t accountId, eServerDisconnectIdentifiers reason) {
		const auto users = UserManager::Instance()->GetUsersForAccount(accountId);
		for (auto* user : users) {
			const SystemAddress sysAddr = user->GetSystemAddress();
			LOG("Dashboard disconnecting account %u (%s)", accountId, user->GetUsername().c_str());
			if (g_LogoutHandler) g_LogoutHandler(sysAddr);
			Game::server->Disconnect(sysAddr, reason);
		}
		return static_cast<uint32_t>(users.size());
	}

	// Pick up a GM level change: lower the in-game level if it now exceeds the account's maximum
	uint32_t RefreshAccount(uint32_t accountId) {
		const auto users = UserManager::Instance()->GetUsersForAccount(accountId);
		if (users.empty()) return 0;

		const auto info = Database::Get()->GetAccountInfo(users.front()->GetUsername());
		if (!info) return 0;

		for (auto* user : users) {
			user->SetMaxGMLevel(info->maxGmLevel);

			auto* entity = PlayerManager::GetPlayer(user->GetSystemAddress());
			if (!entity || entity->GetGMLevel() <= info->maxGmLevel) continue;

			WorldPackets::SendGMLevelChange(entity->GetSystemAddress(), true, info->maxGmLevel, entity->GetGMLevel(), info->maxGmLevel);
			GameMessages::SendChatModeUpdate(entity->GetObjectID(), info->maxGmLevel);
			entity->SetGMLevel(info->maxGmLevel);
			if (info->maxGmLevel == eGameMasterLevel::CIVILIAN) {
				GameMessages::ToggleGMInvisEvent msg;
				msg.Send(entity->GetObjectID());
			}
			GameMessages::SendSlashCommandFeedbackText(entity, u"Your game master level has been changed.");
			LOG("Dashboard lowered GM level of %s to %i", user->GetUsername().c_str(), static_cast<int>(info->maxGmLevel));
		}
		return static_cast<uint32_t>(users.size());
	}

	uint32_t RefreshCharacter(LWOOBJID characterId) {
		auto* entity = PlayerManager::GetPlayer(characterId);
		auto* character = entity ? entity->GetCharacter() : nullptr;
		if (!character) return 0;

		const auto info = Database::Get()->GetCharacterInfo(characterId);
		if (!info) return 0;
		character->SetPermissionMap(info->permissionMap);
		return 1;
	}

	// A moderator's warning, as a popup and in chat, to whichever character of the account is online here
	uint32_t WarnAccount(uint32_t accountId, const std::string& text) {
		uint32_t affected = 0;
		for (auto* player : PlayerManager::GetAllPlayers()) {
			auto* user = player ? UserManager::Instance()->GetUser(player->GetSystemAddress()) : nullptr;
			if (!user || user->GetAccountID() != accountId) continue;
			AMFArrayValue args;
			args.Insert("title", std::string("Warning from a moderator"));
			args.Insert("message", text);
			GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "ToggleAnnounce", args);
			ChatPackets::SendSystemMessage(player->GetSystemAddress(), u"Warning from a moderator: " + GeneralUtils::UTF8ToUTF16(text));
			affected++;
		}
		return affected;
	}

	// Every world gets the request; only the one with both players acts on it
	uint32_t TeleportToPlayer(LWOOBJID characterId, LWOOBJID targetId) {
		auto* entity = PlayerManager::GetPlayer(characterId);
		auto* target = PlayerManager::GetPlayer(targetId);
		if (!entity || !target || entity == target) return 0;
		const auto position = target->GetPosition();
		GameMessages::SendTeleport(entity->GetObjectID(), position, entity->GetRotation(), entity->GetSystemAddress());
		ChatPackets::SendSystemMessage(entity->GetSystemAddress(), u"A moderator moved you to " + GeneralUtils::ASCIIToUTF16(target->GetCharacter() ? target->GetCharacter()->GetName() : "another player") + u".");
		LOG("Dashboard teleport of %llu to %llu", characterId, targetId);
		return 1;
	}

	uint32_t RescueCharacter(LWOOBJID characterId, LWOMAPID zoneId, const std::string& spawnPoint) {
		auto* entity = PlayerManager::GetPlayer(characterId);
		if (!entity || !entity->GetCharacter()) return 0;
		if (!Game::zoneManager->CheckIfAccessibleZone(zoneId)) {
			LOG("Dashboard rescue of %llu to inaccessible zone %u refused", characterId, zoneId);
			return 0;
		}

		ChatPackets::SendSystemMessage(entity->GetSystemAddress(), u"A moderator is moving you to safety...");
		const auto objectId = entity->GetObjectID();
		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, zoneId, 0, false,
			[objectId, spawnPoint](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
				auto* entity = Game::entityManager->GetEntity(objectId);
				if (!entity || !entity->GetCharacter()) return;

				auto* character = entity->GetCharacter();
				if (auto* characterComponent = entity->GetComponent<CharacterComponent>()) {
					characterComponent->AddVisitedLevel(LWOZONEID(zoneID, LWOINSTANCEID_INVALID, zoneClone));
					characterComponent->SetLastRocketConfig(u"");
				}
				character->SetZoneID(zoneID);
				character->SetZoneInstance(zoneInstance);
				character->SetZoneClone(zoneClone);
				// A named spawn point (a respawnname in the zone), as rocket launchers land players; empty is the zone's default
				character->SetTargetScene(spawnPoint);
				character->SaveXMLToDatabase();

				LOG("Dashboard rescue: transferring %llu to zone %u", objectId, zoneID);
				WorldPackets::SendTransferToWorld(entity->GetSystemAddress(), serverIP, serverPort, mythranShift);
			});
		return 1;
	}
}

namespace {
	void Tell(Entity* player, const std::string& message) {
		if (player) ChatPackets::SendSystemMessage(player->GetSystemAddress(), GeneralUtils::UTF8ToUTF16(message));
	}

	uint32_t NameModerated(LWOOBJID characterId, bool approved, const std::string& name) {
		auto* entity = PlayerManager::GetPlayer(characterId);
		auto* character = entity ? entity->GetCharacter() : nullptr;
		if (!character) return 0;
		character->ApplyNameModeration(approved);
		Tell(entity, approved ? "Your new name \"" + name + "\" was approved. Other players will see it after you next change worlds."
			: "Your requested name \"" + name + "\" was not approved. You can pick another one at character select.");
		return 1;
	}

	uint32_t PetNameModerated(LWOOBJID petId, bool approved, const std::string& name) {
		uint32_t affected = 0;
		// A pet that is out updates for everyone at once
		for (auto* player : PlayerManager::GetAllPlayers()) {
			auto* pet = player ? PetComponent::GetActivePet(player->GetObjectID()) : nullptr;
			if (pet && pet->GetDatabaseId() == petId) pet->ApplyNameModeration(approved);
		}
		// Tell whoever owns it
		for (auto* player : PlayerManager::GetAllPlayers()) {
			auto* inventory = player ? player->GetComponent<InventoryComponent>() : nullptr;
			if (!inventory || !inventory->GetPetsMut().contains(petId)) continue;
			Tell(player, approved ? "Your pet's name \"" + name + "\" was approved." : "Your pet's name \"" + name + "\" was not approved. You can give it a new one.");
			affected++;
		}
		return affected;
	}

	uint32_t PropertyModerated(LWOOBJID propertyId, LWOOBJID ownerId, bool approved, const std::string& reason) {
		uint32_t affected = 0;
		if (auto* property = PropertyManagementComponent::Instance(); property && property->GetId() == propertyId) {
			property->ApplyModeration(approved, reason);
			affected++;
		}
		if (auto* owner = PlayerManager::GetPlayer(ownerId)) {
			Tell(owner, approved ? "Your property was approved and can now be visited by others."
				: "Your property was not approved" + (reason.empty() ? std::string(".") : ": " + reason) + " It has been made private.");
			affected++;
		}
		return affected;
	}
}

namespace {
	/**
	 * A moderator changing a mission of a character that is here, as the GM commands do: /completemission (rewards
	 * only when asked), /resetmission and /addmission. Answers 1 when it changed, 2 when there was nothing to change.
	 */
	uint32_t ChangeMission(ePlayerAction action, LWOOBJID characterId, uint32_t missionId, bool rewards, const std::string& name) {
		auto* entity = PlayerManager::GetPlayer(characterId);
		auto* missions = entity ? entity->GetComponent<MissionComponent>() : nullptr;
		if (!missions || !entity->GetCharacter()) return 0;
		auto* mission = missions->GetMission(missionId);
		std::string text;
		switch (action) {
		case ePlayerAction::MISSION_COMPLETE:
			if (mission && mission->IsComplete() && !mission->IsRepeatable()) return 2;
			missions->CompleteMission(missionId, true, rewards);
			text = "A moderator completed \"" + name + "\" for you" + (rewards ? "." : " (without its rewards).");
			break;
		case ePlayerAction::MISSION_RESET:
			if (!mission) return 2;
			missions->ResetMission(missionId);
			text = "A moderator reset \"" + name + "\" so you can do it again.";
			break;
		default:
			if (mission && !mission->IsRepeatable()) return 2;
			missions->AcceptMission(missionId, true);
			text = "A moderator gave you \"" + name + "\".";
			break;
		}
		entity->GetCharacter()->SaveXMLToDatabase();
		Tell(entity, text);
		LOG("Dashboard %s of mission %u for %llu", std::string(magic_enum::enum_name(action)).c_str(), missionId, characterId);
		return 1;
	}
}

void DashboardActions::SetLogoutHandler(std::function<void(const SystemAddress&)> handler) {
	g_LogoutHandler = std::move(handler);
}

uint32_t DashboardActions::Apply(const PlayerActionRequest& request) {
	switch (request.action) {
	case ePlayerAction::KICK_ACCOUNT:
		return KickAccount(request.accountId, static_cast<eServerDisconnectIdentifiers>(request.disconnectReason));
	case ePlayerAction::REFRESH_ACCOUNT:
		return RefreshAccount(request.accountId);
	case ePlayerAction::REFRESH_CHARACTER:
		return RefreshCharacter(request.characterId);
	case ePlayerAction::RESCUE_CHARACTER:
		return RescueCharacter(request.characterId, request.zoneId, request.text);
	case ePlayerAction::NAME_MODERATED:
		return NameModerated(request.characterId, request.approved, request.text);
	case ePlayerAction::PET_NAME_MODERATED:
		return PetNameModerated(request.targetId, request.approved, request.text);
	case ePlayerAction::PROPERTY_MODERATED:
		return PropertyModerated(request.targetId, request.characterId, request.approved, request.text);
	case ePlayerAction::TELEPORT_TO_PLAYER:
		return TeleportToPlayer(request.characterId, request.targetId);
	case ePlayerAction::WARN_ACCOUNT:
		return WarnAccount(request.accountId, request.text);
	case ePlayerAction::CHAT_MESSAGE: {
		// From the dashboard or a chat bridge: shown to everyone in this world (or only the chosen zone or instance), checked by the
		// same chat filter as players' messages unless chat_bridge_filter=0
		if (Game::server->GetZoneID() == 0 || (request.zoneId != 0 && request.zoneId != Game::server->GetZoneID())) return 0;
		if (request.instanceId >= 0 && request.instanceId != Game::server->GetInstanceID()) return 0;
		if (Game::config->GetValue("chat_bridge_filter") != "0" && !Game::chatFilter->IsSentenceOkay(request.text, eGameMasterLevel::CIVILIAN).empty()) return 0;
		const auto players = PlayerManager::GetAllPlayers().size();
		ChatPackets::SendChatMessage(UNASSIGNED_SYSTEM_ADDRESS, static_cast<char>(4), request.name, LWOOBJID_EMPTY, false, GeneralUtils::UTF8ToUTF16(request.text));
		return static_cast<uint32_t>(players);
	}
	case ePlayerAction::RELOAD_VANITY:
		// Character select (zone 0) has no vanity
		if (Game::server->GetZoneID() == 0) return 0;
		VanityUtilities::SpawnVanity();
		return 1;
	case ePlayerAction::RELOAD_CHAT_FILTER:
		if (!Game::chatFilter) return 0;
		Game::chatFilter->ReloadCustomWords();
		return 1;
	case ePlayerAction::MISSION_COMPLETE:
	case ePlayerAction::MISSION_RESET:
	case ePlayerAction::MISSION_ACCEPT:
		return ChangeMission(request.action, request.characterId, static_cast<uint32_t>(request.targetId), request.approved, request.text);
	case ePlayerAction::RELOAD_LIVE_OPS:
		return LiveEvents::Reload();
	}
	return 0;
}
