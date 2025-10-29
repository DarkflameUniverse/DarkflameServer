/*
 * Darkflame Universe
 * Copyright 2019
 */

#include <sstream>
#include <string>

#include "MissionComponent.h"
#include "Logger.h"
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
#include "StringifiedEnum.h"

 // MARK: Mission Component

std::unordered_map<AchievementCacheKey, std::vector<uint32_t>> MissionComponent::m_AchievementCache = {};

//! Initializer
MissionComponent::MissionComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_LastUsedMissionOrderUID = Game::zoneManager->GetUniqueMissionIdStartingValue();

	RegisterMsg(&MissionComponent::OnGetObjectReportInfo);
	RegisterMsg(&MissionComponent::OnGetMissionState);
	RegisterMsg(&MissionComponent::OnMissionNeedsLot);
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


eMissionState MissionComponent::GetMissionState(const uint32_t missionId) const {
	auto* mission = GetMission(missionId);

	if (mission == nullptr) {
		return CanAccept(missionId) ? eMissionState::AVAILABLE : eMissionState::UNKNOWN;
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

void MissionComponent::Progress(eMissionTaskType type, int32_t value, LWOOBJID associate, const std::string& targets, int32_t count, bool ignoreAchievements) {
	LOG("Progressing missions %s %i %llu %s %s", StringifiedEnum::ToString(type).data(), value, associate, targets.c_str(), ignoreAchievements ? "(ignoring achievements)" : "");
	std::vector<uint32_t> acceptedAchievements;
	if (count > 0 && !ignoreAchievements) {
		acceptedAchievements = LookForAchievements(type, value, true, associate, targets, count);
	}

	for (const auto& [id, mission] : m_Missions) {
		if (!mission || std::find(acceptedAchievements.begin(), acceptedAchievements.end(), mission->GetMissionId()) != acceptedAchievements.end()) continue;

		if (mission->IsAchievement() && ignoreAchievements) continue;

		if (mission->IsComplete()) continue;

		mission->Progress(type, value, associate, targets, count);
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
		if (element->GetType() != static_cast<eMissionTaskType>(taskType)) continue;

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
		if (element->GetType() != static_cast<eMissionTaskType>(taskType) || !element->InAllTargets(value)) continue;

		element->AddProgress(1);
	}

	if (!mission->IsComplete()) {
		mission->CheckCompletion();
	}
}

bool MissionComponent::GetMissionInfo(uint32_t missionId, CDMissions& result) {
	auto* missionsTable = CDClientManager::GetTable<CDMissionsTable>();

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

const std::vector<uint32_t> MissionComponent::LookForAchievements(eMissionTaskType type, int32_t value, bool progress, LWOOBJID associate, const std::string& targets, int32_t count) {
#ifdef MISSION_NEW_METHOD
	// Query for achievments, using the cache
	const auto& result = QueryAchievements(type, value, targets);

	std::vector<uint32_t> acceptedAchievements;

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

		acceptedAchievements.push_back(missionID);

		if (progress) {
			// Progress mission to bring it up to speed
			instance->Progress(type, value, associate, targets, count);
		}
	}

	return acceptedAchievements;
#else
	auto* missionTasksTable = CDClientManager::GetTable<CDMissionTasksTable>();
	auto* missionsTable = CDClientManager::GetTable<CDMissionsTable>();

	auto tasks = missionTasksTable->Query([=](const CDMissionTasks& entry) {
		return entry.taskType == static_cast<unsigned>(type);
		});

	std::vector<uint32_t> acceptedAchievements;

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
					LOG("Failed to parse target (%s): (%s)!", token.c_str(), exception.what());
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

		acceptedAchievements.push_back(mission.id);

		if (progress) {
			instance->Progress(type, value, associate, targets, count);
		}
	}

	return acceptedAchievements;
#endif
}

const std::vector<uint32_t>& MissionComponent::QueryAchievements(eMissionTaskType type, int32_t value, const std::string targets) {
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
	auto* missionTasksTable = CDClientManager::GetTable<CDMissionTasksTable>();
	auto* missionsTable = CDClientManager::GetTable<CDMissionsTable>();

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
	m_AchievementCache.insert_or_assign(toFind, result);
	return m_AchievementCache.find(toFind)->second;
}

bool MissionComponent::RequiresItem(const LOT lot) {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT type FROM Objects WHERE id = ?;");
	query.bind(1, static_cast<int>(lot));

	auto result = query.execQuery();

	if (result.eof()) {
		return false;
	}

	if (!result.fieldIsNull("type")) {
		const auto type = std::string(result.getStringField("type"));

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
			if (task->IsComplete() || task->GetType() != eMissionTaskType::GATHER) {
				continue;
			}

			if (!task->InAllTargets(lot)) {
				continue;
			}

			return true;
		}
	}

	const auto required = LookForAchievements(eMissionTaskType::GATHER, lot, false);

	return !required.empty();
}


void MissionComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	auto* mis = doc.FirstChildElement("obj")->FirstChildElement("mis");

	if (mis == nullptr) return;

	auto* cur = mis->FirstChildElement("cur");
	auto* done = mis->FirstChildElement("done");

	auto* doneM = done->FirstChildElement();

	while (doneM) {
		int missionId;

		doneM->QueryAttribute("id", &missionId);

		auto* mission = new Mission(this, missionId);

		mission->LoadFromXmlDone(*doneM);

		doneM = doneM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}

	auto* currentM = cur->FirstChildElement();

	uint32_t missionOrder{};
	while (currentM) {
		int missionId;

		currentM->QueryAttribute("id", &missionId);

		auto* mission = m_Missions.contains(missionId) ? m_Missions[missionId] : new Mission(this, missionId);

		mission->LoadFromXmlCur(*currentM);

		if (currentM->QueryAttribute("o", &missionOrder) == tinyxml2::XML_SUCCESS && mission->IsMission()) {
			mission->SetUniqueMissionOrderID(missionOrder);
			if (missionOrder > m_LastUsedMissionOrderUID) m_LastUsedMissionOrderUID = missionOrder;
		}

		currentM = currentM->NextSiblingElement();

		m_Missions.insert_or_assign(missionId, mission);
	}
}


void MissionComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	auto shouldInsertMis = false;

	auto* obj = doc.FirstChildElement("obj");

	auto* mis = obj->FirstChildElement("mis");

	if (mis == nullptr) {
		mis = doc.NewElement("mis");

		shouldInsertMis = true;
	}

	mis->DeleteChildren();

	auto* done = doc.NewElement("done");
	auto* cur = doc.NewElement("cur");

	for (const auto& pair : m_Missions) {
		auto* mission = pair.second;

		if (mission) {
			const auto completions = mission->GetCompletions();

			auto* m = doc.NewElement("m");

			if (completions > 0) {
				mission->UpdateXmlDone(*m);

				done->LinkEndChild(m);

				if (mission->IsComplete()) continue;

				m = doc.NewElement("m");
			}

			if (mission->IsMission()) m->SetAttribute("o", mission->GetUniqueMissionOrderID());

			mission->UpdateXmlCur(*m);

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

void MissionComponent::ResetMission(const int32_t missionId) {
	auto* mission = GetMission(missionId);

	if (!mission) return;

	m_Missions.erase(missionId);
	GameMessages::SendResetMissions(m_Parent, m_Parent->GetSystemAddress(), missionId);
}

void PushMissions(const std::map<uint32_t, Mission*>& missions, AMFArrayValue& V, bool verbose) {
	for (const auto& [id, mission] : missions) {
		std::stringstream ss;
		if (!mission) {
			ss << "Mission ID: " << id;
			V.PushDebug(ss.str());
		} else if (!verbose) {
			ss << "%[Missions_" << id << "_name]" << ", Mission ID";
			V.PushDebug<AMFIntValue>(ss.str()) = id;
		} else {
			ss << "%[Missions_" << id << "_name]" << ", Mission ID: " << id;
			auto& missionV = V.PushDebug(ss.str());
			auto& missionInformation = missionV.PushDebug("Mission Information");

			if (mission->IsComplete()) {
				missionInformation.PushDebug<AMFStringValue>("Time mission last completed") = std::to_string(mission->GetTimestamp());
				missionInformation.PushDebug<AMFIntValue>("Number of times completed") = mission->GetCompletions();
			}
			// Expensive to network this especially when its read from the client anyways
			// missionInformation.PushDebug("Description").PushDebug("None");
			// missionInformation.PushDebug("Text").PushDebug("None");

			auto& statusInfo = missionInformation.PushDebug("Mission statuses for local player");
			if (mission->IsAvalible()) statusInfo.PushDebug("Available");
			if (mission->IsActive()) statusInfo.PushDebug("Active");
			if (mission->IsReadyToComplete()) statusInfo.PushDebug("Ready To Complete");
			if (mission->IsComplete()) statusInfo.PushDebug("Completed");
			if (mission->IsFailed()) statusInfo.PushDebug("Failed");
			const auto& clientInfo = mission->GetClientInfo();

			statusInfo.PushDebug<AMFBoolValue>("Is an achievement mission") = mission->IsAchievement();
			statusInfo.PushDebug<AMFBoolValue>("Is an timed mission") = clientInfo.time_limit > 0;
			auto& taskInfo = statusInfo.PushDebug("Task Info");
			taskInfo.PushDebug<AMFIntValue>("Number of tasks in this mission") = mission->GetTasks().size();
			int32_t i = 0;
			for (const auto* task : mission->GetTasks()) {
				auto& thisTask = taskInfo.PushDebug("Task " + std::to_string(i));
				// Expensive to network this especially when its read from the client anyways
				// thisTask.PushDebug("Description").PushDebug("%[MissionTasks_" + taskUidStr + "_description]");
				thisTask.PushDebug<AMFIntValue>("Number done") = std::min(task->GetProgress(), static_cast<uint32_t>(task->GetClientInfo().targetValue));
				thisTask.PushDebug<AMFIntValue>("Number total needed") = task->GetClientInfo().targetValue;
				thisTask.PushDebug<AMFIntValue>("Task Type") = task->GetClientInfo().taskType;
				i++;
			}


			// auto& chatText = missionInformation.PushDebug("Chat Text for Mission States");
			// Expensive to network this especially when its read from the client anyways
			// chatText.PushDebug("Available Text").PushDebug("%[MissionText_" + idStr + "_chat_state_1]");
			// chatText.PushDebug("Active Text").PushDebug("%[MissionText_" + idStr + "_chat_state_2]");
			// chatText.PushDebug("Ready-to-Complete Text").PushDebug("%[MissionText_" + idStr + "_chat_state_3]");
			// chatText.PushDebug("Complete Text").PushDebug("%[MissionText_" + idStr + "_chat_state_4]");

			if (clientInfo.time_limit > 0) {
				missionInformation.PushDebug<AMFIntValue>("Time Limit") = clientInfo.time_limit;
				missionInformation.PushDebug<AMFDoubleValue>("Time Remaining") = 0;
			}

			if (clientInfo.offer_objectID != -1) {
				missionInformation.PushDebug<AMFIntValue>("Offer Object LOT") = clientInfo.offer_objectID;
			}

			if (clientInfo.target_objectID != -1) {
				missionInformation.PushDebug<AMFIntValue>("Complete Object LOT") = clientInfo.target_objectID;
			}

			if (!clientInfo.prereqMissionID.empty()) {
				missionInformation.PushDebug<AMFStringValue>("Requirement Mission IDs") = clientInfo.prereqMissionID;
			}

			missionInformation.PushDebug<AMFBoolValue>("Is Repeatable") = clientInfo.repeatable;
			const bool hasNoOfferer = clientInfo.offer_objectID == -1 || clientInfo.offer_objectID == 0;
			const bool hasNoCompleter = clientInfo.target_objectID == -1 || clientInfo.target_objectID == 0;
			missionInformation.PushDebug<AMFBoolValue>("Is Achievement") = hasNoOfferer && hasNoCompleter;
		}
	}
}

bool MissionComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	auto& missionInfo = reportInfo.info->PushDebug("Mission (Laggy)");
	missionInfo.PushDebug<AMFIntValue>("Component ID") = GetComponentID();
	// Sort the missions so they are easier to parse and present to the end user
	std::map<uint32_t, Mission*> achievements;
	std::map<uint32_t, Mission*> missions;
	std::map<uint32_t, Mission*> doneMissions;
	for (const auto [id, mission] : m_Missions) {
		if (!mission) continue;
		else if (mission->IsComplete()) doneMissions[id] = mission;
		else if (mission->IsAchievement()) achievements[id] = mission;
		else if (mission->IsMission()) missions[id] = mission;
	}

	// None of these should be empty, but if they are dont print the field
	if (!achievements.empty() || !missions.empty()) {
		auto& incompleteMissions = missionInfo.PushDebug("Incomplete Missions");
		PushMissions(achievements, incompleteMissions, reportInfo.bVerbose);
		PushMissions(missions, incompleteMissions, reportInfo.bVerbose);
	}

	if (!doneMissions.empty()) {
		auto& completeMissions = missionInfo.PushDebug("Completed Missions");
		PushMissions(doneMissions, completeMissions, reportInfo.bVerbose);
	}

	return true;
}

bool MissionComponent::OnGetMissionState(GameMessages::GetMissionState& getMissionState) {
	getMissionState.missionState = GetMissionState(getMissionState.missionID);

	return true;
}

bool MissionComponent::OnMissionNeedsLot(GameMessages::MissionNeedsLot& missionNeedsLot) {
	return RequiresItem(missionNeedsLot.item);
}
