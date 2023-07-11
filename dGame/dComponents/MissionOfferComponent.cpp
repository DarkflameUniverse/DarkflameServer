/*
 * Darkflame Universe
 * Copyright 2023
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
#include "eMissionState.h"

#include "CDComponentsRegistryTable.h"

MissionOfferComponent::MissionOfferComponent(Entity* parent, const int32_t componentId) : Component(parent) {
	m_ComponentId = componentId;
}

void MissionOfferComponent::LoadTemplateData() {
	if (m_ComponentId == -1) return;
	// Now lookup the missions in the MissionNPCComponent table
	auto* missionNpcComponentTable = CDClientManager::Instance().GetTable<CDMissionNPCComponentTable>();

	auto missions = missionNpcComponentTable->Query([=](const CDMissionNPCComponent& entry) {
		return entry.id == static_cast<int32_t>(m_ComponentId);
		});

	for (const auto& mission : missions) {
		this->offeredMissions.emplace_back(
			std::make_unique<OfferedMission>(mission.missionID, mission.offersMission, mission.acceptsMission)
		);
	}
}

void MissionOfferComponent::OnUse(Entity* originator) {
	OfferMissions(originator);
}

void MissionOfferComponent::OfferMissions(Entity* entity, const uint32_t specifiedMissionId) {
	// First, get the entity's MissionComponent. If there is not one, then we cannot offer missions to this entity.
	auto* missionComponent = entity->GetComponent<MissionComponent>();

	if (!missionComponent) {
		Game::logger->Log("MissionOfferComponent", "Unable to get mission component for Entity %llu", entity->GetObjectID());
		return;
	}

	std::vector<uint32_t> offered{};

	CDMissions missionInfo{};

	if (specifiedMissionId > 0 && !Mission::IsValidMission(specifiedMissionId, missionInfo)) {
		return;
	}

	for (const auto& offeredMission : offeredMissions) {
		if (specifiedMissionId > 0 && (offeredMission->GetMissionId() != specifiedMissionId && !missionInfo.isRandom)) {
			continue;
		}

		// First, check if we already have the mission
		const auto missionId = offeredMission->GetMissionId();

		auto* mission = missionComponent->GetMission(missionId);

		if (mission) {
			if (specifiedMissionId <= 0) {
				// Handles the odd case where the offer object should not display the mission again
				if (!mission->IsComplete() &&
					mission->GetClientInfo().offer_objectID == m_ParentEntity->GetLOT() &&
					mission->GetClientInfo().target_objectID != m_ParentEntity->GetLOT() &&
					mission->IsFetchMission()) {
					continue;
				}
			}

			// We have the mission, if it is not complete, offer it
			if (mission->IsActive() || mission->IsReadyToComplete()) {
				GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), missionId, m_ParentEntity->GetObjectID());

				offered.push_back(missionId);

				continue;
			}
		}

		const auto canAccept = MissionPrerequisites::CanAccept(missionId, missionComponent->GetMissions());

		// Mission has not yet been accepted - check the prereqs
		if (!canAccept || !Mission::IsValidMission(missionId, missionInfo)) continue;

		// This means the mission is part of a random pool of missions.
		if (missionInfo.isRandom && missionInfo.randomPool.empty()) continue;

		if (missionInfo.isRandom && !missionInfo.randomPool.empty()) {
			auto randomMissionPoolStr = GeneralUtils::SplitString(missionInfo.randomPool, ',');

			std::vector<uint32_t> randomMissions;
			for (const auto& randomMissionStr : randomMissionPoolStr) {
				uint32_t randomMission;
				if (GeneralUtils::TryParse(randomMissionStr, randomMission)) randomMissions.push_back(randomMission);
			}

			if (specifiedMissionId > 0) {
				if (std::find(randomMissions.begin(), randomMissions.end(), specifiedMissionId) != randomMissions.end() &&
					MissionPrerequisites::CanAccept(specifiedMissionId, missionComponent->GetMissions())) {
					GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), specifiedMissionId, m_ParentEntity->GetObjectID());
					return;
				}
			}

			std::vector<uint32_t> canAcceptPool;
			for (const auto& sample : randomMissions) {
				const auto state = missionComponent->GetMissionState(sample);

				if (state == eMissionState::ACTIVE ||
					state == eMissionState::COMPLETE_ACTIVE ||
					state == eMissionState::READY_TO_COMPLETE ||
					state == eMissionState::COMPLETE_READY_TO_COMPLETE ||
					sample == specifiedMissionId) {
					mission = missionComponent->GetMission(sample);

					if (!mission || mission->IsAchievement()) continue;

					GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), sample, m_ParentEntity->GetObjectID());

					canAcceptPool.clear();

					break;
				}

				if (std::find(offered.begin(), offered.end(), sample) == offered.end() &&
					MissionPrerequisites::CanAccept(sample, missionComponent->GetMissions())) {
					canAcceptPool.push_back(sample);
				}
			}

			// If the mission is already active or we already completed one of them today
			if (canAcceptPool.empty()) continue;

			const auto selected = canAcceptPool[GeneralUtils::GenerateRandomNumber<int32_t>(0, canAcceptPool.size() - 1)];

			GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), selected, m_ParentEntity->GetObjectID());
		} else if (std::find(offered.begin(), offered.end(), missionId) == offered.end() && offeredMission->GetOfferMission()) {
			GameMessages::SendOfferMission(entity->GetObjectID(), entity->GetSystemAddress(), missionId, m_ParentEntity->GetObjectID());
		}
	}
}
