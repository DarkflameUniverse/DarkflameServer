/*
 * Darkflame Universe
 * Copyright 2023
 */

#include <string>

#include "MissionComponent.h"
#include "dLogger.h"
#include "CDClientManager.h"
#include "CDMissionTasksTable.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Game.h"
#include "Amf3.h"
#include "dZoneManager.h"
#include "Mail.h"
#include "MissionPrerequisites.h"
#include "AchievementCacheKey.h"
#include "eMissionState.h"
#include "GeneralUtils.h"

std::unordered_map<AchievementCacheKey, std::vector<uint32_t>> MissionComponent::m_AchievementCache = {};

MissionComponent::MissionComponent(Entity* parent) : Component(parent) {
	m_LastUsedMissionOrderUID = dZoneManager::Instance()->GetUniqueMissionIdStartingValue();
}

MissionComponent::~MissionComponent() {
	for (const auto& [missionId, mission] : m_Missions) {
		delete mission;
	}
}

Mission* MissionComponent::GetMission(const uint32_t missionId) const {
	if (m_Missions.count(missionId) == 0) return nullptr;

	const auto& index = m_Missions.find(missionId);

	return index == m_Missions.end() ? nullptr : index->second;
}

eMissionState MissionComponent::GetMissionState(const uint32_t missionId) const {
	auto* mission = GetMission(missionId);

	if (!mission) return CanAccept(missionId) ? eMissionState::AVAILABLE : eMissionState::UNKNOWN;

	return mission->GetMissionState();
}

bool MissionComponent::CanAccept(const uint32_t missionId) const {
	return MissionPrerequisites::CanAccept(missionId, m_Missions);
}

void MissionComponent::AcceptMission(const uint32_t missionId, const bool skipChecks) {
	if (!skipChecks && !CanAccept(missionId)) {
		return;
	}

	// If this is a daily mission, it may already be "accepted"
	auto* mission = this->GetMission(missionId);

	if (mission) {
		if (mission->GetClientInfo().repeatable) {
			mission->Accept();
			if (mission->IsMission()) mission->SetUniqueMissionOrderID(++m_LastUsedMissionOrderUID);
		}

		return;
	}

	mission = new Mission(this, missionId);

	if (mission->IsMission()) mission->SetUniqueMissionOrderID(++m_LastUsedMissionOrderUID);

	mission->Accept();

	this->m_Missions.insert_or_assign(missionId, mission);

	//Needs to send a mail
	if (missionId == 1728) Mail::HandleNotificationRequest(m_ParentEntity->GetSystemAddress(), m_ParentEntity->GetObjectID());
}

void MissionComponent::CompleteMission(const uint32_t missionId, const bool skipChecks, const bool yieldRewards) {
	// Get the mission first
	auto* mission = this->GetMission(missionId);

	if (!mission) {
		AcceptMission(missionId, skipChecks);

		mission = this->GetMission(missionId);

		if (!mission) return;
	}

	//If this mission is not repeatable, and already completed, we stop here.
	if (mission->IsComplete() && !mission->IsRepeatable()) return;

	mission->Complete(yieldRewards);
}

void MissionComponent::RemoveMission(const uint32_t missionId) {
	auto missionItr = m_Missions.find(missionId);

	if (missionItr == m_Missions.end()) return;

	delete missionItr->second;

	m_Missions.erase(missionItr);
}

void MissionComponent::Progress(const eMissionTaskType type, const int32_t value, const LWOOBJID& associate, const std::string& targets, const int32_t count, const bool ignoreAchievements) {
	for (const auto& [missionId, mission] : m_Missions) {
		if (!mission) continue;
		if (mission->IsAchievement() && ignoreAchievements) continue;

		if (mission->IsComplete()) continue;

		mission->Progress(type, value, associate, targets, count);
	}

	if (count > 0 && !ignoreAchievements) {
		LookForAchievements(type, value, true, associate, targets, count);
	}
}

void MissionComponent::ForceProgress(const uint32_t missionId, const uint32_t taskId, const int32_t value, const bool acceptMission) {
	auto* mission = GetMission(missionId);

	if (!mission) {
		if (!acceptMission) return;

		AcceptMission(missionId);

		mission = GetMission(missionId);

		if (!mission) return;
	}

	std::for_each(mission->GetTasks().begin(), mission->GetTasks().end(), [value, taskId](MissionTask* element) {
		if (element->GetClientInfo().uid != taskId) return;

		element->AddProgress(value);
		});

	if (!mission->IsComplete()) mission->CheckCompletion();
}

void MissionComponent::ForceProgressTaskType(const uint32_t missionId, const uint32_t taskType, const int32_t value, const bool acceptMission) {
	auto* mission = GetMission(missionId);

	if (!mission) {
		if (!acceptMission) return;

		CDMissions missionInfo;

		if (!GetMissionInfo(missionId, missionInfo)) return;

		if (missionInfo.isMission) return;

		AcceptMission(missionId);

		mission = GetMission(missionId);

		if (!mission) return;
	}

	std::for_each(mission->GetTasks().begin(), mission->GetTasks().end(), [value, taskType](MissionTask* element) {
		if (element->GetType() != static_cast<eMissionTaskType>(taskType)) return;

		element->AddProgress(value);
		});

	if (!mission->IsComplete()) mission->CheckCompletion();
}

void MissionComponent::ForceProgressValue(const uint32_t missionId, const uint32_t taskType, const int32_t value, const bool acceptMission) {
	auto* mission = GetMission(missionId);

	if (!mission) {
		if (!acceptMission) {
			return;
		}

		CDMissions missionInfo;

		if (!GetMissionInfo(missionId, missionInfo)) {
			return;
		}

		if (missionInfo.isMission) {
			return;
		}

		AcceptMission(missionId);

		mission = GetMission(missionId);

		if (mission == nullptr) {
			return;
		}
	}

	for (auto* element : mission->GetTasks()) {
		if (element->GetType() != static_cast<eMissionTaskType>(taskType) || !element->InAllTargets(value)) continue;

		element->AddProgress(1);
	}

	if (!mission->IsComplete()) {
		mission->CheckCompletion();
	}
}

bool MissionComponent::GetMissionInfo(const uint32_t missionId, CDMissions& result) const {
	auto* missionsTable = CDClientManager::Instance().GetTable<CDMissionsTable>();

	const auto missions = missionsTable->Query([=](const CDMissions& entry) {
		return entry.id == static_cast<uint32_t>(missionId);
		});

	if (missions.empty()) {
		return false;
	}

	result = missions[0];

	return true;
}

bool MissionComponent::LookForAchievements(const eMissionTaskType type, const int32_t value, const bool progress, const LWOOBJID& associate, const std::string& targets, const int32_t count) {
	// Query for achievments, using the cache
	const auto& result = QueryAchievements(type, value, targets);

	bool any = false;

	for (const uint32_t missionID : result) {
		// Check if we already have this achievement
		if (GetMission(missionID)) continue;

		// Check if we can accept this achievement
		if (!MissionPrerequisites::CanAccept(missionID, m_Missions)) {
			continue;
		}

		// Instantiate new mission and accept it
		auto* instance = new Mission(this, missionID);

		m_Missions.insert_or_assign(missionID, instance);

		if (instance->IsMission()) instance->SetUniqueMissionOrderID(++m_LastUsedMissionOrderUID);

		instance->Accept();

		any = true;

		if (!progress) continue;
		// Progress mission to bring it up to speed
		instance->Progress(type, value, associate, targets, count);
	}

	return any;
}

const std::vector<uint32_t>& MissionComponent::QueryAchievements(const eMissionTaskType type, const int32_t value, const std::string& targets) {
	// Create a hash which represent this query for achievements
	AchievementCacheKey toFind;
	toFind.SetType(type);
	toFind.SetValue(value);
	toFind.SetTargets(targets);

	const auto& iter = m_AchievementCache.find(toFind);

	// Check if this query is cached
	if (iter != m_AchievementCache.end()) {
		return iter->second;
	}

	// Find relevent tables
	auto* missionTasksTable = CDClientManager::Instance().GetTable<CDMissionTasksTable>();
	auto* missionsTable = CDClientManager::Instance().GetTable<CDMissionsTable>();

	std::vector<uint32_t> result;

	// Loop through all mission tasks, might cache this task check later
	for (const auto& task : missionTasksTable->GetEntries()) {
		if (task.taskType != static_cast<uint32_t>(type)) {
			continue;
		}

		// Seek the assosicated mission
		auto foundMission = false;

		const auto& cdMission = missionsTable->GetByMissionID(task.id, foundMission);

		if (!foundMission || cdMission.isMission) continue;

		// Compare the easy values
		if (task.target == value || task.targetGroup == targets) {
			result.push_back(cdMission.id);

			continue;
		}

		// Compare the target group, array separated by ','
		for (const auto& possibleMissionStr : GeneralUtils::SplitString(task.targetGroup, ',')) {
			uint32_t possibleMission;
			if (GeneralUtils::TryParse(possibleMissionStr, possibleMission) && possibleMission == value) {
				result.push_back(cdMission.id);

				break;
			}
		}
	}
	// Insert into cache and return the inserted value.
	return m_AchievementCache.insert_or_assign(toFind, result).first->second;
}

bool MissionComponent::RequiresItem(const LOT lot) {
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT type FROM Objects WHERE id = ?;");
	query.bind(1, static_cast<int>(lot));

	auto result = query.execQuery();

	if (result.eof()) return false;

	if (!result.fieldIsNull(0)) {
		const auto type = std::string(result.getStringField(0));

		if (type == "Powerup") return true;
	}

	for (const auto& [missionId, mission] : m_Missions) {
		if (mission->IsComplete()) continue;

		for (auto* task : mission->GetTasks()) {
			if (task->IsComplete() || task->GetType() != eMissionTaskType::GATHER) {
				continue;
			}

			if (!task->InAllTargets(lot)) continue;

			return true;
		}
	}
	return LookForAchievements(eMissionTaskType::GATHER, lot, false);
}


void MissionComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	if (!doc) return;

	auto* mis = doc->FirstChildElement("obj")->FirstChildElement("mis");

	if (!mis) return;

	auto* cur = mis->FirstChildElement("cur");
	auto* done = mis->FirstChildElement("done");

	auto* doneM = done->FirstChildElement();

	while (doneM) {
		uint32_t missionId;

		doneM->QueryAttribute("id", &missionId);

		auto* mission = new Mission(this, missionId);

		mission->LoadFromXml(doneM);

		doneM = doneM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}

	auto* currentM = cur->FirstChildElement();

	uint32_t missionOrder{};
	while (currentM) {
		uint32_t missionId;

		currentM->QueryAttribute("id", &missionId);

		auto* mission = new Mission(this, missionId);

		mission->LoadFromXml(currentM);

		if (currentM->QueryAttribute("o", &missionOrder) == tinyxml2::XML_SUCCESS && mission->IsMission()) {
			mission->SetUniqueMissionOrderID(missionOrder);
			m_LastUsedMissionOrderUID = std::max(missionOrder, m_LastUsedMissionOrderUID);
		}

		currentM = currentM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}
}


void MissionComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	if (!doc) return;

	auto shouldInsertMis = false;

	auto* obj = doc->FirstChildElement("obj");

	auto* mis = obj->FirstChildElement("mis");

	if (!mis) {
		mis = doc->NewElement("mis");

		shouldInsertMis = true;
	}

	mis->DeleteChildren();

	auto* done = doc->NewElement("done");
	auto* cur = doc->NewElement("cur");

	for (const auto& [missionId, mission] : m_Missions) {
		if (!mission) continue;
		const auto complete = mission->IsComplete();

		auto* missionElement = doc->NewElement("m");

		if (!complete && mission->IsMission()) missionElement->SetAttribute("o", mission->GetUniqueMissionOrderID());

		mission->UpdateXml(missionElement);

		cur->LinkEndChild(missionElement);
	}

	mis->InsertFirstChild(done);
	mis->InsertEndChild(cur);

	if (shouldInsertMis) obj->LinkEndChild(mis);
}

void MissionComponent::AddCollectible(const int32_t collectibleID) {
	if (!HasCollectible(collectibleID)) m_Collectibles.push_back(collectibleID);
}

bool MissionComponent::HasCollectible(const int32_t collectibleID) const {
	return std::find(m_Collectibles.begin(), m_Collectibles.end(), collectibleID) != m_Collectibles.end();
}

bool MissionComponent::HasMission(const uint32_t missionId) const {
	return GetMission(missionId) != nullptr;
}
