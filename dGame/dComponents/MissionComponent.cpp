/*
 * Darkflame Universe
 * Copyright 2019
 */

#include <sstream>
#include <string>

#include "MissionComponent.h"
#include "dLogger.h"
#include "CDClientManager.h"
#include "CDMissionTasksTable.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Game.h"
#include "AMFFormat.h"
#include "dZoneManager.h"
#include "Mail.h"
#include "MissionPrerequisites.h"

 // MARK: Mission Component

std::unordered_map<size_t, std::vector<uint32_t>> MissionComponent::m_AchievementCache = {};

//! Initializer
MissionComponent::MissionComponent(Entity* parent) : Component(parent) {
	m_LastUsedMissionOrderUID = dZoneManager::Instance()->GetUniqueMissionIdStartingValue();
}

//! Destructor
MissionComponent::~MissionComponent() {
	for (const auto& mission : m_Missions) {
		delete mission.second;
	}

	this->m_Missions.clear();
}


Mission* MissionComponent::GetMission(const uint32_t missionId) const {
	if (m_Missions.count(missionId) == 0) {
		return nullptr;
	}

	const auto& index = m_Missions.find(missionId);

	if (index == m_Missions.end()) {
		return nullptr;
	}

	return index->second;
}


MissionState MissionComponent::GetMissionState(const uint32_t missionId) const {
	auto* mission = GetMission(missionId);

	if (mission == nullptr) {
		return CanAccept(missionId) ? MissionState::MISSION_STATE_AVAILABLE : MissionState::MISSION_STATE_UNKNOWN;
	}

	return mission->GetMissionState();
}


const std::unordered_map<uint32_t, Mission*>& MissionComponent::GetMissions() const {
	return m_Missions;
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

	if (mission != nullptr) {
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

	if (missionId == 1728) {
		//Needs to send a mail

		auto address = m_Parent->GetSystemAddress();

		Mail::HandleNotificationRequest(address, m_Parent->GetObjectID());
	}
}


void MissionComponent::CompleteMission(const uint32_t missionId, const bool skipChecks, const bool yieldRewards) {
	// Get the mission first
	auto* mission = this->GetMission(missionId);

	if (mission == nullptr) {
		AcceptMission(missionId, skipChecks);

		mission = this->GetMission(missionId);

		if (mission == nullptr) {
			return;
		}
	}

	//If this mission is not repeatable, and already completed, we stop here.
	if (mission->IsComplete() && !mission->IsRepeatable()) {
		return;
	}

	mission->Complete(yieldRewards);
}

void MissionComponent::RemoveMission(uint32_t missionId) {
	auto* mission = this->GetMission(missionId);

	if (mission == nullptr) {
		return;
	}

	delete mission;

	m_Missions.erase(missionId);
}

void MissionComponent::Progress(MissionTaskType type, int32_t value, LWOOBJID associate, const std::string& targets, int32_t count, bool ignoreAchievements) {
	for (const auto& pair : m_Missions) {
		auto* mission = pair.second;

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

	if (mission == nullptr) {
		if (!acceptMission) {
			return;
		}

		AcceptMission(missionId);

		mission = GetMission(missionId);

		if (mission == nullptr) {
			return;
		}
	}

	for (auto* element : mission->GetTasks()) {
		if (element->GetClientInfo().uid != taskId) continue;

		element->AddProgress(value);
	}

	if (!mission->IsComplete()) {
		mission->CheckCompletion();
	}
}

void MissionComponent::ForceProgressTaskType(const uint32_t missionId, const uint32_t taskType, const int32_t value, const bool acceptMission) {
	auto* mission = GetMission(missionId);

	if (mission == nullptr) {
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
		if (element->GetType() != static_cast<MissionTaskType>(taskType)) continue;

		element->AddProgress(value);
	}

	if (!mission->IsComplete()) {
		mission->CheckCompletion();
	}
}

void MissionComponent::ForceProgressValue(uint32_t missionId, uint32_t taskType, int32_t value, bool acceptMission) {
	auto* mission = GetMission(missionId);

	if (mission == nullptr) {
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
		if (element->GetType() != static_cast<MissionTaskType>(taskType) || !element->InAllTargets(value)) continue;

		element->AddProgress(1);
	}

	if (!mission->IsComplete()) {
		mission->CheckCompletion();
	}
}

bool MissionComponent::GetMissionInfo(uint32_t missionId, CDMissions& result) {
	auto* missionsTable = CDClientManager::Instance()->GetTable<CDMissionsTable>("Missions");

	const auto missions = missionsTable->Query([=](const CDMissions& entry) {
		return entry.id == static_cast<int>(missionId);
		});

	if (missions.empty()) {
		return false;
	}

	result = missions[0];

	return true;
}

#define MISSION_NEW_METHOD

bool MissionComponent::LookForAchievements(MissionTaskType type, int32_t value, bool progress, LWOOBJID associate, const std::string& targets, int32_t count) {
#ifdef MISSION_NEW_METHOD
	// Query for achievments, using the cache
	const auto& result = QueryAchievements(type, value, targets);

	bool any = false;

	for (const uint32_t missionID : result) {
		// Check if we already have this achievement
		if (GetMission(missionID) != nullptr) {
			continue;
		}

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

		if (progress) {
			// Progress mission to bring it up to speed
			instance->Progress(type, value, associate, targets, count);
		}
	}

	return any;
#else
	auto* missionTasksTable = CDClientManager::Instance()->GetTable<CDMissionTasksTable>("MissionTasks");
	auto* missionsTable = CDClientManager::Instance()->GetTable<CDMissionsTable>("Missions");

	auto tasks = missionTasksTable->Query([=](const CDMissionTasks& entry) {
		return entry.taskType == static_cast<unsigned>(type);
		});

	auto any = false;

	for (const auto& task : tasks) {
		if (GetMission(task.id) != nullptr) {
			continue;
		}

		const auto missionEntries = missionsTable->Query([=](const CDMissions& entry) {
			return entry.id == static_cast<int>(task.id) && !entry.isMission;
			});

		if (missionEntries.empty()) {
			continue;
		}

		const auto mission = missionEntries[0];

		if (mission.isMission || !MissionPrerequisites::CanAccept(mission.id, m_Missions)) {
			continue;
		}

		if (task.target != value && task.targetGroup != targets) {
			auto stream = std::istringstream(task.targetGroup);
			std::string token;

			auto found = false;

			while (std::getline(stream, token, ',')) {
				try {
					const auto target = std::stoul(token);

					found = target == value;

					if (found) {
						break;
					}
				} catch (std::invalid_argument& exception) {
					Game::logger->Log("MissionComponent", "Failed to parse target (%s): (%s)!", token.c_str(), exception.what());
				}
			}

			if (!found) {
				continue;
			}
		}

		auto* instance = new Mission(this, mission.id);

		m_Missions.insert_or_assign(mission.id, instance);

		if (instance->IsMission()) instance->SetUniqueMissionOrderID(++m_LastUsedMissionOrderUID);

		instance->Accept();

		any = true;

		if (progress) {
			instance->Progress(type, value, associate, targets, count);
		}
	}

	return any;
#endif
}

const std::vector<uint32_t>& MissionComponent::QueryAchievements(MissionTaskType type, int32_t value, const std::string targets) {
	// Create a hash which represent this query for achievements
	size_t hash = 0;
	GeneralUtils::hash_combine(hash, type);
	GeneralUtils::hash_combine(hash, value);
	GeneralUtils::hash_combine(hash, targets);

	const std::unordered_map<size_t, std::vector<uint32_t>>::iterator& iter = m_AchievementCache.find(hash);

	// Check if this query is cached
	if (iter != m_AchievementCache.end()) {
		return iter->second;
	}

	// Find relevent tables
	auto* missionTasksTable = CDClientManager::Instance()->GetTable<CDMissionTasksTable>("MissionTasks");
	auto* missionsTable = CDClientManager::Instance()->GetTable<CDMissionsTable>("Missions");

	std::vector<uint32_t> result;

	// Loop through all mission tasks, might cache this task check later
	for (const auto& task : missionTasksTable->GetEntries()) {
		if (task.taskType != static_cast<uint32_t>(type)) {
			continue;
		}

		// Seek the assosicated mission
		auto foundMission = false;

		const auto& mission = missionsTable->GetByMissionID(task.id, foundMission);

		if (!foundMission || mission.isMission) {
			continue;
		}

		// Compare the easy values
		if (task.target == value || task.targetGroup == targets) {
			result.push_back(mission.id);

			continue;
		}

		// Compare the target group, array separated by ','
		auto stream = std::istringstream(task.targetGroup);
		std::string token;

		while (std::getline(stream, token, ',')) {
			try {
				if (std::stoi(token) == value) {
					result.push_back(mission.id);

					continue;
				}
			} catch (std::invalid_argument& exception) {
				// Ignored
			}
		}
	}

	// Insert into cache
	m_AchievementCache.insert_or_assign(hash, result);

	return m_AchievementCache.find(hash)->second;
}

bool MissionComponent::RequiresItem(const LOT lot) {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT type FROM Objects WHERE id = ?;");
	query.bind(1, (int)lot);

	auto result = query.execQuery();

	if (result.eof()) {
		return false;
	}

	if (!result.fieldIsNull(0)) {
		const auto type = std::string(result.getStringField(0));

		result.finalize();

		if (type == "Powerup") {
			return true;
		}
	}

	result.finalize();

	for (const auto& pair : m_Missions) {
		auto* mission = pair.second;

		if (mission->IsComplete()) {
			continue;
		}

		for (auto* task : mission->GetTasks()) {
			if (task->IsComplete() || task->GetType() != MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION) {
				continue;
			}

			if (!task->InAllTargets(lot)) {
				continue;
			}

			return true;
		}
	}

	const auto required = LookForAchievements(MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION, lot, false);

	return required;
}


void MissionComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	if (doc == nullptr) return;

	auto* mis = doc->FirstChildElement("obj")->FirstChildElement("mis");

	if (mis == nullptr) return;

	auto* cur = mis->FirstChildElement("cur");
	auto* done = mis->FirstChildElement("done");

	auto* doneM = done->FirstChildElement();

	while (doneM) {
		int missionId;

		doneM->QueryAttribute("id", &missionId);

		auto* mission = new Mission(this, missionId);

		mission->LoadFromXml(doneM);

		doneM = doneM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}

	auto* currentM = cur->FirstChildElement();

	uint32_t missionOrder{};
	while (currentM) {
		int missionId;

		currentM->QueryAttribute("id", &missionId);

		auto* mission = new Mission(this, missionId);

		mission->LoadFromXml(currentM);

		if (currentM->QueryAttribute("o", &missionOrder) == tinyxml2::XML_SUCCESS && mission->IsMission()) {
			mission->SetUniqueMissionOrderID(missionOrder);
			if (missionOrder > m_LastUsedMissionOrderUID) m_LastUsedMissionOrderUID = missionOrder;
		}

		currentM = currentM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}
}


void MissionComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	if (doc == nullptr) return;

	auto shouldInsertMis = false;

	auto* obj = doc->FirstChildElement("obj");

	auto* mis = obj->FirstChildElement("mis");

	if (mis == nullptr) {
		mis = doc->NewElement("mis");

		shouldInsertMis = true;
	}

	mis->DeleteChildren();

	auto* done = doc->NewElement("done");
	auto* cur = doc->NewElement("cur");

	for (const auto& pair : m_Missions) {
		auto* mission = pair.second;

		if (mission) {
			const auto complete = mission->IsComplete();

			auto* m = doc->NewElement("m");

			if (complete) {
				mission->UpdateXml(m);

				done->LinkEndChild(m);

				continue;
			}
			if (mission->IsMission()) m->SetAttribute("o", mission->GetUniqueMissionOrderID());

			mission->UpdateXml(m);

			cur->LinkEndChild(m);
		}
	}

	mis->InsertFirstChild(done);
	mis->InsertEndChild(cur);

	if (shouldInsertMis) {
		obj->LinkEndChild(mis);
	}
}

void MissionComponent::AddCollectible(int32_t collectibleID) {
	// Check if this collectible is already in the list
	if (HasCollectible(collectibleID)) {
		return;
	}

	m_Collectibles.push_back(collectibleID);
}

bool MissionComponent::HasCollectible(int32_t collectibleID) {
	return std::find(m_Collectibles.begin(), m_Collectibles.end(), collectibleID) != m_Collectibles.end();
}

bool MissionComponent::HasMission(uint32_t missionId) {
	return GetMission(missionId) != nullptr;
}
