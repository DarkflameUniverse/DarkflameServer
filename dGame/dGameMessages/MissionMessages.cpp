#include "MissionMessages.h"

#include "BitStreamUtils.h"
#include "Character.h"
#include "CppScripts.h"
#include "dConfig.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"
#include "ePlayerFlag.h"
#include "eReplicaComponentType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "Mission.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"

namespace GameMessages {
	void OfferMission::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(missionID);
		bitStream.Write(offerer);
	}

	bool OfferMission::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(offerer));
		return true;
	}

	void RespondToMission::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(missionID);
		bitStream.Write(playerID);
		bitStream.Write(receiver);
		BitStreamUtils::WriteOptional(bitStream, rewardItem, LOT_NULL);
	}

	bool RespondToMission::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(receiver));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, rewardItem, LOT_NULL));
		return true;
	}

	void RespondToMission::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto reward = rewardItem;
		MissionComponent* missionComponent = static_cast<MissionComponent*>(entity.GetComponent(eReplicaComponentType::MISSION));
		if (!missionComponent) {
			LOG("Unable to get mission component for entity %llu to handle RespondToMission", playerID);
			return;
		}

		Mission* mission = missionComponent->GetMission(missionID);
		if (mission) {
			mission->SetReward(reward);
		} else {
			LOG("Unable to get mission %i for entity %llu to update reward in RespondToMission", missionID, playerID);
		}

		Entity* offerer = Game::entityManager->GetEntity(receiver);

		if (offerer == nullptr) {
			LOG("Unable to get receiver entity %llu for RespondToMission", receiver);
			return;
		}

		offerer->GetScript()->OnRespondToMission(offerer, missionID, Game::entityManager->GetEntity(playerID), reward);
	}

	void NotifyMission::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(missionID);
		bitStream.Write(missionState);
		bitStream.Write(sendingRewards);
	}

	bool NotifyMission::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(missionState));
		VALIDATE_READ(bitStream.Read(sendingRewards));
		return true;
	}

	void NotifyMissionTask::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(missionID);
		bitStream.Write(taskMask);
		bitStream.Write<uint8_t>(updates.size());
		for (const auto update : updates) bitStream.Write(update);
	}

	bool NotifyMissionTask::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(taskMask));
		uint8_t count{};
		VALIDATE_READ(bitStream.Read(count));
		updates.resize(count);
		for (auto& update : updates) VALIDATE_READ(bitStream.Read(update));
		return true;
	}

	void ResetMissions::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, missionID, -1);
	}

	bool ResetMissions::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, missionID, -1));
		return true;
	}

	void MissionDialogueOK::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bIsComplete);
		bitStream.Write(iMissionState);
		bitStream.Write(missionID);
		bitStream.Write(responder);
	}

	bool MissionDialogueOK::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bIsComplete));
		VALIDATE_READ(bitStream.Read(iMissionState));
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(responder));
		return true;
	}

	void MissionDialogueOK::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		Entity* player = Game::entityManager->GetEntity(responder);
		if (!player) {
			LOG("MissionDialogueOK for mission %i from unknown responder %llu", missionID, responder);
			return;
		}

		entity->GetScript()->OnMissionDialogueOK(entity, player, missionID, iMissionState);

		// Get the player's mission component
		MissionComponent* missionComponent = static_cast<MissionComponent*>(player->GetComponent(eReplicaComponentType::MISSION));
		if (!missionComponent) {
			LOG("Unable to get mission component for entity %llu to handle MissionDialogueOK", player->GetObjectID());
			return;
		}

		if (iMissionState == eMissionState::AVAILABLE || iMissionState == eMissionState::COMPLETE_AVAILABLE) {
			missionComponent->AcceptMission(missionID);
		} else if (iMissionState == eMissionState::READY_TO_COMPLETE || iMissionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
			missionComponent->CompleteMission(missionID);
		}

		if (Game::config->GetValue("allow_players_to_skip_cinematics") != "1"
			|| !player->GetCharacter()
			|| !player->GetCharacter()->GetPlayerFlag(ePlayerFlag::DLU_SKIP_CINEMATICS)) return;
		player->AddCallbackTimer(0.5f, [player]() {
			if (!player) return;
			GameMessages::SendEndCinematic(player->GetObjectID(), u"", player->GetSystemAddress());
			});
	}

	void RequestLinkedMission::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(missionID);
		bitStream.Write(bMissionOffered);
	}

	bool RequestLinkedMission::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(missionID));
		VALIDATE_READ(bitStream.Read(bMissionOffered));
		return true;
	}

	void RequestLinkedMission::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = Game::entityManager->GetEntity(playerID);

		auto* missionOfferComponent = static_cast<MissionOfferComponent*>(entity.GetComponent(eReplicaComponentType::MISSION_OFFER));

		if (missionOfferComponent != nullptr) {
			missionOfferComponent->OfferMissions(player, 0);
		}
	}

	void SetFlag::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFlag);
		bitStream.Write(iFlagID);
	}

	bool SetFlag::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFlag));
		VALIDATE_READ(bitStream.Read(iFlagID));
		return true;
	}

	void SetFlag::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto character = entity.GetCharacter();
		if (character) character->SetPlayerFlag(iFlagID, bFlag);

		// This is always set the first time a player loads into a world from character select
		// and is used to know when to refresh the players inventory items so they show up.
		if (iFlagID == ePlayerFlag::IS_NEWS_SCREEN_VISIBLE && bFlag) {
			entity.SetVar<bool>(u"dlu_first_time_load", true);
		}
	}

	void NotifyClientFlagChange::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFlag);
		bitStream.Write(iFlagID);
	}

	bool NotifyClientFlagChange::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFlag));
		VALIDATE_READ(bitStream.Read(iFlagID));
		return true;
	}

	void HasBeenCollected::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool HasBeenCollected::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void HasBeenCollected::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Entity* player = Game::entityManager->GetEntity(playerID);
		if (!player || entity.GetCollectibleID() == 0) return;

		MissionComponent* missionComponent = static_cast<MissionComponent*>(player->GetComponent(eReplicaComponentType::MISSION));
		if (missionComponent) {
			missionComponent->Progress(eMissionTaskType::COLLECTION, entity.GetLOT(), entity.GetObjectID());
		}
	}

	void NotifyLevelRewards::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(level);
		bitStream.Write(sendingRewards);
	}

	bool NotifyLevelRewards::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(level));
		VALIDATE_READ(bitStream.Read(sendingRewards));
		return true;
	}
}
