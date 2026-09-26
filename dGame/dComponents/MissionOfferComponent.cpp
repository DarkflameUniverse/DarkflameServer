/*
 * Darkflame Universe
 * Copyright 2019
 */

#include <sstream>
#include "MissionOfferComponent.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "CDMissionNPCComponentTable.h"
#include "GameMessages.h"
#include "MissionMessages.h"
#include "Entity.h"
#include "MissionComponent.h"
#include "Logger.h"
#include "Game.h"
#include "MissionPrerequisites.h"
#include "eMissionState.h"

#include "CDComponentsRegistryTable.h"

OfferedMission::OfferedMission(const uint32_t missionId, const bool offersMission, const bool acceptsMission) {
	this->missionId = missionId;
	this->offersMission = offersMission;
	this->acceptsMission = acceptsMission;
}


uint32_t OfferedMission::GetMissionId() const {
	return this->missionId;
}

bool OfferedMission::GetOffersMission() const {
	return this->offersMission;
}

bool OfferedMission::GetAcceptsMission() const {
	return this->acceptsMission;
}

//------------------------ MissionOfferComponent below ------------------------

MissionOfferComponent::MissionOfferComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	if (componentID != -1) {
		// Now lookup the missions in the MissionNPCComponent table
		auto* missionNpcComponentTable = CDClientManager::GetTable<CDMissionNPCComponentTable>();

		auto missions = missionNpcComponentTable->Query([componentID](const CDMissionNPCComponent& entry) {
			return entry.id == static_cast<unsigned>(componentID);
			});

		for (auto& mission : missions) {
			this->offeredMissions.emplace_back(mission.missionID, mission.offersMission, mission.acceptsMission);
		}
	}
}

namespace {
	// "Why are we sending it twice, once to a non-player object?"
	// The first one (targeting the offerer) makes the client zoom into the object.
	// The second (targeting the player) actually makes the UI pop up so the mission can be offered.
	// Why is it like this? Because LU isn't just a clown, it's the entire circus.
	void OfferMissionToPlayer(const Entity& player, const int32_t missionID, const LWOOBJID offerer) {
		GameMessages::OfferMission offerMission;
		offerMission.missionID = missionID;
		offerMission.offerer = offerer;

		offerMission.target = offerer;
		offerMission.SendToClient(player.GetSystemAddress());

		offerMission.target = player.GetObjectID();
		offerMission.SendToClient(player.GetSystemAddress());
	}
}

void MissionOfferComponent::OnUse(Entity* originator) {
	OfferMissions(originator);
}

void MissionOfferComponent::OfferMissions(Entity* entity, const uint32_t specifiedMissionId) {
	// First, get the entity's MissionComponent. If there is not one, then we cannot offer missions to this entity.
	auto* missionComponent = entity->GetComponent<MissionComponent>();

	if (!missionComponent) {
		LOG("Unable to get mission component for Entity %llu", entity->GetObjectID());
		return;
	}

	CDMissions info{};

	if (specifiedMissionId > 0 && !Mission::IsValidMission(specifiedMissionId, info)) {
		return;
	}

	for (const auto offeredMission : this->offeredMissions) {
		if (specifiedMissionId > 0) {
			if (offeredMission.GetMissionId() != specifiedMissionId && !info.isRandom) {
				continue;
			}
		}

		// First, check if we already have the mission
		const auto missionId = offeredMission.GetMissionId();

		auto* mission = missionComponent->GetMission(missionId);

		if (mission != nullptr) {
			if (specifiedMissionId <= 0) {
				// Handles the odd case where the offer object should not display the mission again
				if (!mission->IsComplete() && mission->GetClientInfo().offer_objectID == m_Parent->GetLOT() && mission->GetClientInfo().target_objectID != m_Parent->GetLOT() && mission->IsFetchMission()) {
					continue;
				}
			}

			// We have the mission, if it is not complete, offer it
			if (mission->IsActive() || mission->IsReadyToComplete()) {
				OfferMissionToPlayer(*entity, missionId, m_Parent->GetObjectID());

				continue;
			}
		}

		const auto canAccept = MissionPrerequisites::CanAccept(missionId, missionComponent->GetMissions());

		// Mission has not yet been accepted - check the prereqs
		if (!canAccept || !Mission::IsValidMission(missionId, info)) continue;

		const auto& randomPool = info.randomPool;
		const auto isRandom = info.isRandom;

		// This means the mission is part of a random pool of missions.
		if (isRandom && randomPool.empty()) continue;

		if (isRandom && !randomPool.empty()) {
			std::istringstream stream(randomPool);
			std::string token;

			std::vector<uint32_t> randomMissionPool;

			while (std::getline(stream, token, ',')) {
				try {
					const auto value = std::stoul(token);

					randomMissionPool.push_back(value);
				} catch (std::invalid_argument& exception) {
					LOG("Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
				}
			}

			if (specifiedMissionId > 0) {
				const auto& iter = std::find(randomMissionPool.begin(), randomMissionPool.end(), specifiedMissionId);

				if (iter != randomMissionPool.end() && MissionPrerequisites::CanAccept(specifiedMissionId, missionComponent->GetMissions())) {
					OfferMissionToPlayer(*entity, specifiedMissionId, m_Parent->GetObjectID());

					return;
				}
			}

			std::vector<uint32_t> canAcceptPool;

			for (const auto sample : randomMissionPool) {
				const auto state = missionComponent->GetMissionState(sample);

				if (state == eMissionState::ACTIVE ||
					state == eMissionState::COMPLETE_ACTIVE ||
					state == eMissionState::READY_TO_COMPLETE ||
					state == eMissionState::COMPLETE_READY_TO_COMPLETE ||
					sample == specifiedMissionId) {
					mission = missionComponent->GetMission(sample);

					if (mission == nullptr || mission->IsAchievement()) continue;

					OfferMissionToPlayer(*entity, sample, m_Parent->GetObjectID());

					canAcceptPool.clear();

					break;
				}

				if (MissionPrerequisites::CanAccept(sample, missionComponent->GetMissions())) {
					canAcceptPool.push_back(sample);
				}
			}

			// If the mission is already active or we already completed one of them today
			if (canAcceptPool.empty()) continue;

			const auto selected = canAcceptPool[GeneralUtils::GenerateRandomNumber<int>(0, canAcceptPool.size() - 1)];

			OfferMissionToPlayer(*entity, selected, m_Parent->GetObjectID());
		} else if (offeredMission.GetOffersMission()) {
			OfferMissionToPlayer(*entity, missionId, m_Parent->GetObjectID());
		}
	}
}
