#include <sstream>

#include "MissionTask.h"

#include "Game.h"
#include "dLogger.h"
#include "Mission.h"
#include "Character.h"
#include "dServer.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "MissionComponent.h"


MissionTask::MissionTask(Mission* mission, CDMissionTasks* info, uint32_t mask) {
	this->info = info;
	this->mission = mission;
	this->mask = mask;

	progress = 0;

	std::istringstream stream(info->taskParam1);
	std::string token;

	while (std::getline(stream, token, ',')) {
		uint32_t parameter;
		if (GeneralUtils::TryParse(token, parameter)) {
			parameters.push_back(parameter);
		}
	}

	stream = std::istringstream(info->targetGroup);

	while (std::getline(stream, token, ',')) {
		uint32_t parameter;
		if (GeneralUtils::TryParse(token, parameter)) {
			targets.push_back(parameter);
		}
	}
}


MissionTaskType MissionTask::GetType() const {
	return static_cast<MissionTaskType>(info->taskType);
}


uint32_t MissionTask::GetProgress() const {
	return progress;
}


void MissionTask::SetProgress(const uint32_t value, const bool echo) {
	if (progress == value) {
		return;
	}

	progress = value;

	if (!echo) {
		return;
	}

	auto* entity = mission->GetAssociate();

	if (entity == nullptr) {
		return;
	}

	std::vector<float> updates;
	updates.push_back(static_cast<float>(progress));
	GameMessages::SendNotifyMissionTask(entity, entity->GetSystemAddress(), static_cast<int>(info->id), static_cast<int>(1 << (mask + 1)), updates);
}


void MissionTask::SetUnique(const std::vector<uint32_t>& value) {
	unique = value;
}


void MissionTask::AddProgress(int32_t value) {
	value += progress;

	if (value > info->targetValue) {
		value = info->targetValue;
	}

	if (value < 0) {
		value = 0;
	}

	SetProgress(value);
}


Mission* MissionTask::GetMission() const {
	return mission;
}


uint32_t MissionTask::GetTarget() const {
	return info->target;
}


const CDMissionTasks& MissionTask::GetClientInfo() const {
	return *info;
}


uint32_t MissionTask::GetMask() const {
	return mask;
}


const std::vector<uint32_t>& MissionTask::GetUnique() const {
	return unique;
}


const std::vector<uint32_t>& MissionTask::GetTargets() const {
	return targets;
}


const std::vector<uint32_t>& MissionTask::GetParameters() const {
	return parameters;
}


std::vector<uint32_t> MissionTask::GetAllTargets() const {
	auto targets = GetTargets();

	targets.push_back(GetTarget());

	return targets;
}


bool MissionTask::InTargets(const uint32_t value) const {
	auto targets = GetTargets();

	return std::find(targets.begin(), targets.end(), value) != targets.end();
}


bool MissionTask::InAllTargets(const uint32_t value) const {
	auto targets = GetAllTargets();

	return std::find(targets.begin(), targets.end(), value) != targets.end();
}

bool MissionTask::InParameters(const uint32_t value) const {
	auto parameters = GetParameters();

	return std::find(parameters.begin(), parameters.end(), value) != parameters.end();
}


bool MissionTask::IsComplete() const {
	// Mission 668 has task uid 984 which is a bit mask.  Its completion value is 3.
	if (info->uid == 984) {
		return progress >= 3;
	} else {
		return progress >= info->targetValue;
	}
}


void MissionTask::Complete() {
	SetProgress(info->targetValue);
}


void MissionTask::CheckCompletion() const {
	if (IsComplete()) {
		mission->CheckCompletion();
	}
}


void MissionTask::Progress(int32_t value, LWOOBJID associate, const std::string& targets, int32_t count) {
	if (IsComplete() && count > 0) return;

	const auto type = GetType();

	if (count < 0) {
		if (mission->IsMission() && type == MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION && InAllTargets(value)) {
			if (parameters.size() > 0 && (parameters[0] & 1) != 0) {
				return;
			}

			auto* inventoryComponent = mission->GetAssociate()->GetComponent<InventoryComponent>();

			if (inventoryComponent != nullptr) {
				int32_t itemCount = inventoryComponent->GetLotCountNonTransfer(value);

				if (itemCount < info->targetValue) {
					SetProgress(itemCount);

					if (mission->IsReadyToComplete()) {
						mission->MakeActive();
					}
				}
			}
		}

		return;
	}

	Entity* entity;
	ScriptedActivityComponent* activity;
	uint32_t activityId;
	uint32_t lot;
	uint32_t collectionId;
	std::vector<LDFBaseData*> settings;

	switch (type) {
	case MissionTaskType::MISSION_TASK_TYPE_UNKNOWN:
		break;

	case MissionTaskType::MISSION_TASK_TYPE_ACTIVITY:
	{
		if (InAllTargets(value)) {
			AddProgress(count);
			break;
		}

		entity = EntityManager::Instance()->GetEntity(associate);
		if (entity == nullptr) {
			if (associate != LWOOBJID_EMPTY) {
				Game::logger->Log("MissionTask", "Failed to find associated entity (%llu)!", associate);
			}
			break;
		}

		activity = static_cast<ScriptedActivityComponent*>(entity->GetComponent(COMPONENT_TYPE_REBUILD));
		if (activity == nullptr) {
			break;
		}

		activityId = activity->GetActivityID();

		const auto activityIdOverride = entity->GetVar<int32_t>(u"activityID");

		if (activityIdOverride != 0) {
			activityId = activityIdOverride;
		}

		if (!InAllTargets(activityId)) break;

		AddProgress(count);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_FOOD:
	case MissionTaskType::MISSION_TASK_TYPE_MISSION_INTERACTION:
	{
		if (GetTarget() != value) break;

		AddProgress(count);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_EMOTE:
	{
		if (!InParameters(value)) break;

		entity = EntityManager::Instance()->GetEntity(associate);

		if (entity == nullptr) {
			Game::logger->Log("MissionTask", "Failed to find associated entity (%llu)!", associate);

			break;
		}

		lot = static_cast<uint32_t>(entity->GetLOT());

		if (GetTarget() != lot) break;

		AddProgress(count);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_SKILL:
	{
		// This is a complicated check because for some missions we need to check for the associate being in the parameters instead of the value being in the parameters.
		if (associate == LWOOBJID_EMPTY && GetAllTargets().size() == 1 && GetAllTargets()[0] == -1) {
			if (InParameters(value)) AddProgress(count);
		} else {
			if (InParameters(associate) && InAllTargets(value)) AddProgress(count);
		}
		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_MINIGAME:
	{
		auto* minigameManager = EntityManager::Instance()->GetEntity(associate);
		if (minigameManager == nullptr)
			break;

		int32_t gameID = minigameManager->GetLOT();

		auto* sac = minigameManager->GetComponent<ScriptedActivityComponent>();
		if (sac != nullptr) {
			gameID = sac->GetActivityID();
		}

		if (info->target != gameID) {
			break;
		}
		// This special case is for shooting gallery missions that want their
		// progress value set to 1 instead of being set to the target value.
		if (info->targetGroup == targets && value >= info->targetValue && GetMission()->IsMission() && info->target == 1864 && info->targetGroup == "performact_score") {
			SetProgress(1);
			break;
		}
		if (info->targetGroup == targets && value >= info->targetValue) {
			SetProgress(info->targetValue);
			break;
		}
		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_VISIT_PROPERTY:
	{
		if (!InAllTargets(value)) break;

		if (std::find(unique.begin(), unique.end(), static_cast<uint32_t>(associate)) != unique.end()) break;

		AddProgress(count);

		unique.push_back(associate);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_ENVIRONMENT:
	{
		if (!InAllTargets(value)) break;

		entity = EntityManager::Instance()->GetEntity(associate);

		if (entity == nullptr) {
			Game::logger->Log("MissionTask", "Failed to find associated entity (%llu)!", associate);

			break;
		}

		collectionId = entity->GetCollectibleID();

		collectionId = static_cast<uint32_t>(collectionId) + static_cast<uint32_t>(Game::server->GetZoneID() << 8);

		if (std::find(unique.begin(), unique.end(), collectionId) != unique.end()) break;

		unique.push_back(collectionId);

		SetProgress(unique.size());

		auto* entity = mission->GetAssociate();

		if (entity == nullptr) break;

		auto* missionComponent = entity->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) break;

		missionComponent->AddCollectible(collectionId);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_LOCATION:
	{
		if (info->targetGroup != targets) break;

		AddProgress(count);

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_RACING:
	{
		// The meaning of associate can be found in RacingTaskParam.h
		if (parameters.empty()) break;

		if (!InAllTargets(dZoneManager::Instance()->GetZone()->GetWorldID()) && !(parameters[0] == 4 || parameters[0] == 5) && !InAllTargets(value)) break;

		if (parameters[0] != associate) break;

		if (associate == 1 || associate == 2 || associate == 3) {
			if (value > info->targetValue) break;

			AddProgress(info->targetValue);
		}
		// task 15 is a bit mask!
		else if (associate == 15) {
			if (!InAllTargets(value)) break;

			auto tempProgress = GetProgress();
			// If we won at Nimbus Station, set bit 0
			if (value == 1203) SetProgress(tempProgress |= 1 << 0);
			// If we won at Gnarled Forest, set bit 1
			else if (value == 1303) SetProgress(tempProgress |= 1 << 1);
			// If both bits are set, then the client sees the mission as complete.
		} else if (associate == 10) {
			// If the player did not crash during the race, progress this task by count.
			if (value != 0) break;

			AddProgress(count);
		} else if (associate == 4 || associate == 5 || associate == 14) {
			if (!InAllTargets(value)) break;
			AddProgress(count);
		} else if (associate == 17) {
			if (!InAllTargets(value)) break;
			AddProgress(count);
		} else {
			AddProgress(count);
		}

		break;
	}

	case MissionTaskType::MISSION_TASK_TYPE_PET_TAMING:
	case MissionTaskType::MISSION_TASK_TYPE_SCRIPT:
	case MissionTaskType::MISSION_TASK_TYPE_NON_MISSION_INTERACTION:
	case MissionTaskType::MISSION_TASK_TYPE_MISSION_COMPLETE:
	case MissionTaskType::MISSION_TASK_TYPE_POWERUP:
	case MissionTaskType::MISSION_TASK_TYPE_SMASH:
	case MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION:
	case MissionTaskType::MISSION_TASK_TYPE_PLAYER_FLAG:
	case MissionTaskType::MISSION_TASK_TYPE_EARN_REPUTATION:
	{
		if (!InAllTargets(value)) break;

		AddProgress(count);

		break;
	}
	case MissionTaskType::MISSION_TASK_TYPE_PLACE_MODEL:
	{
		AddProgress(count);
		break;
	}
	default:
		Game::logger->Log("MissionTask", "Invalid mission task type (%i)!", static_cast<int>(type));
		return;
	}

	CheckCompletion();
}


MissionTask::~MissionTask() {
	targets.clear();

	parameters.clear();

	unique.clear();
}
