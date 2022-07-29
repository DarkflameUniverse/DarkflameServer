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
#include "Entity.h"
#include "MissionComponent.h"
#include "dLogger.h"
#include "Game.h"
#include "MissionPrerequisites.h"

OfferedMission::OfferedMission(const uint32_t missionId, const bool offersMission, const bool acceptsMission) {
	this->missionId = missionId;
	this->offersMission = offersMission;
	this->acceptsMission = acceptsMission;
}


uint32_t OfferedMission::GetMissionId() const {
	return this->missionId;
}

bool OfferedMission::GetOfferMission() const {
	return this->offersMission;
}

bool OfferedMission::GetAcceptMission() const {
	return this->acceptsMission;
}

//------------------------ MissionOfferComponent below ------------------------

MissionOfferComponent::MissionOfferComponent(Entity* parent, const LOT parentLot) : Component(parent) {
	auto* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	auto value = compRegistryTable->GetByIDAndType(parentLot, COMPONENT_TYPE_MISSION_OFFER, -1);

	if (value != -1) {
		const uint32_t componentId = value;

		// Now lookup the missions in the MissionNPCComponent table
		auto* missionNpcComponentTable = CDClientManager::Instance()->GetTable<CDMissionNPCComponentTable>("MissionNPCComponent");

		auto missions = missionNpcComponentTable->Query([=](const CDMissionNPCComponent& entry) {
			return entry.id == static_cast<unsigned>(componentId);
			});

		for (auto& mission : missions) {
			auto* offeredMission = new OfferedMission(mission.missionID, mission.offersMission, mission.acceptsMission);
			this->offeredMissions.push_back(offeredMission);
		}
	}
}


MissionOfferComponent::~MissionOfferComponent() {
	for (auto* mission : this->offeredMissions) {
		if (mission) {
			delete mission;
			mission = nullptr;
		}
	}

	offeredMissions.clear();
}

void MissionOfferComponent::OnUse(Entity* originator) {
	OfferMissions(originator);
}

void MissionOfferComponent::OfferMissions(Entity* entity, const uint32_t specifiedMissionId) {
	// First, get the entity's MissionComponent. If there is not one, then we cannot offer missions to this entity.
	auto* missionComponent = static_cast<MissionComponent*>(entity->GetComponent(COMPONENT_TYPE_MISSION));

	if (!missionComponent) {
		Game::logger->Log("MissionOfferComponent", "Unable to get mission component for Entity %llu", entity->GetObjectID());
		return;
	}

	std::vector<uint32_t> offered{};

	CDMissions info{};

	if (specifiedMissionId > 0 && !Mission::IsValidMission(specifiedMissionId, info)) {
		return;
	}

	for (auto* offeredMission : this->offeredMissions) {
		if (specifiedMissionId > 0) {
			if (offeredMission->GetMissionId() != specifiedMissionId && !info.isRandom) {
				continue;
			}
		}

		// First, check if we already have the mission
		const auto missionId = offeredMission->GetMissionId();

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
				GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), missionId, m_Parent->GetObjectID());

				offered.push_back(missionId);

				continue;
			}
		}

		const auto canAccept = MissionPrerequisites::CanAccept(missionId, missionComponent->GetMissions());

		// Mission has not yet been accepted - check the prereqs
		if (!canAccept)
			continue;

		if (!Mission::IsValidMission(missionId, info)) {
			continue;
		}

		const auto& randomPool = info.randomPool;
		const auto isRandom = info.isRandom;

		if (isRandom && randomPool.empty()) // This means the mission is part of a random pool of missions.
		{
			continue;
		}

		if (isRandom && !randomPool.empty()) {
			std::istringstream stream(randomPool);
			std::string token;

			std::vector<uint32_t> randomMissionPool;

			while (std::getline(stream, token, ',')) {
				try {
					const auto value = std::stoul(token);

					randomMissionPool.push_back(value);
				} catch (std::invalid_argument& exception) {
					Game::logger->Log("MissionOfferComponent", "Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
				}
			}

			if (specifiedMissionId > 0) {
				const auto& iter = std::find(randomMissionPool.begin(), randomMissionPool.end(), specifiedMissionId);

				if (iter != randomMissionPool.end() && MissionPrerequisites::CanAccept(specifiedMissionId, missionComponent->GetMissions())) {
					GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), specifiedMissionId, m_Parent->GetObjectID());

					return;
				}
			}

			std::vector<uint32_t> canAcceptPool;

			for (const auto sample : randomMissionPool) {
				const auto state = missionComponent->GetMissionState(sample);

				if (state == MissionState::MISSION_STATE_ACTIVE ||
					state == MissionState::MISSION_STATE_COMPLETE_ACTIVE ||
					state == MissionState::MISSION_STATE_READY_TO_COMPLETE ||
					state == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE ||
					sample == specifiedMissionId) {
					mission = missionComponent->GetMission(sample);

					if (mission == nullptr || mission->IsAchievement()) {
						continue;
					}

					GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), sample, m_Parent->GetObjectID());

					canAcceptPool.clear();

					break;
				}

				if (std::find(offered.begin(), offered.end(), sample) == offered.end() && MissionPrerequisites::CanAccept(sample, missionComponent->GetMissions())) {
					canAcceptPool.push_back(sample);
				}
			}

			// If the mission is already active or we already completed one of them today
			if (canAcceptPool.empty())
				continue;

			const auto selected = canAcceptPool[GeneralUtils::GenerateRandomNumber<int>(0, canAcceptPool.size() - 1)];

			GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), selected, m_Parent->GetObjectID());
		} else if (std::find(offered.begin(), offered.end(), missionId) == offered.end() && offeredMission->GetOfferMission()) {
			GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), missionId, m_Parent->GetObjectID());
		}
	}
}
