#include "TriggerComponent.h"
#include "dZoneManager.h"
#include "LUTriggers.h"
#include "eTriggerCommandType.h"
#include "MissionComponent.h"
#include "PhantomPhysicsComponent.h"
#include "CDMissionTasksTable.h"

TriggerComponent::TriggerComponent(Entity* parent, const std::string triggerInfo): Component(parent) {
	m_Parent = parent;
	m_Trigger = nullptr;

	std::vector<std::string> tokens = GeneralUtils::SplitString(triggerInfo, ':');

	uint32_t sceneID;
	GeneralUtils::TryParse<uint32_t>(tokens.at(0), sceneID);
	uint32_t triggerID;
	GeneralUtils::TryParse<uint32_t>(tokens.at(1), triggerID);

	m_Trigger = dZoneManager::Instance()->GetZone()->GetTrigger(sceneID, triggerID);

	if (!m_Trigger) m_Trigger = new LUTriggers::Trigger();
}

void TriggerComponent::TriggerEvent(eTriggerEventType event, Entity* optionalTarget) {
	if (m_Trigger && m_Trigger->enabled) {
		for (LUTriggers::Event* triggerEvent : m_Trigger->events) {
			if (triggerEvent->id == event) {
				for (LUTriggers::Command* command : triggerEvent->commands) {
					HandleTriggerCommand(command, optionalTarget);
				}
			}
		}
	}
}

void TriggerComponent::HandleTriggerCommand(LUTriggers::Command* command, Entity* optionalTarget) {
	auto argArray =  GeneralUtils::SplitString(command->args, ',');

	// determine targets
	std::vector<Entity*> targetEntities = GatherTargets(command, optionalTarget);

	// if we have no targets, then we are done
	if (targetEntities.empty()) return;

	for (Entity* targetEntity : targetEntities) {
		if (!targetEntity) continue;

		switch (command->id) {
			case eTriggerCommandType::ZONE_PLAYER: break;
			case eTriggerCommandType::FIRE_EVENT:
				HandleFireEvent(targetEntity, command->args);
				break;
			case eTriggerCommandType::DESTROY_OBJ: break;
			case eTriggerCommandType::TOGGLE_TRIGGER: break;
			case eTriggerCommandType::RESET_REBUILD: break;
			case eTriggerCommandType::SET_PATH: break;
			case eTriggerCommandType::SET_PICK_TYPE: break;
			case eTriggerCommandType::MOVE_OBJECT: break;
			case eTriggerCommandType::ROTATE_OBJECT: break;
			case eTriggerCommandType::PUSH_OBJECT: break;
			case eTriggerCommandType::REPEL_OBJECT: break;
			case eTriggerCommandType::SET_TIMER: break;
			case eTriggerCommandType::CANCEL_TIMER: break;
			case eTriggerCommandType::PLAY_CINEMATIC: break;
			case eTriggerCommandType::TOGGLE_BBB: break;
			case eTriggerCommandType::UPDATE_MISSION:
				HandleUpdateMission(targetEntity, argArray);
				break;
			case eTriggerCommandType::SET_BOUNCER_STATE: break;
			case eTriggerCommandType::BOUNCE_ALL_ON_BOUNCER: break;
			case eTriggerCommandType::TURN_AROUND_ON_PATH: break;
			case eTriggerCommandType::GO_FORWARD_ON_PATH: break;
			case eTriggerCommandType::GO_BACKWARD_ON_PATH: break;
			case eTriggerCommandType::STOP_PATHING: break;
			case eTriggerCommandType::START_PATHING: break;
			case eTriggerCommandType::LOCK_OR_UNLOCK_CONTROLS: break;
			case eTriggerCommandType::PLAY_EFFECT: break;
			case eTriggerCommandType::STOP_EFFECT: break;
			case eTriggerCommandType::ACTIVATE_MUSIC_CUE: break;
			case eTriggerCommandType::DEACTIVATE_MUSIC_CUE: break;
			case eTriggerCommandType::FLASH_MUSIC_CUE: break;
			case eTriggerCommandType::SET_MUSIC_PARAMETER: break;
			case eTriggerCommandType::PLAY_2D_AMBIENT_SOUND: break;
			case eTriggerCommandType::STOP_2D_AMBIENT_SOUND: break;
			case eTriggerCommandType::PLAY_3D_AMBIENT_SOUND: break;
			case eTriggerCommandType::STOP_3D_AMBIENT_SOUND: break;
			case eTriggerCommandType::ACTIVATE_MIXER_PROGRAM: break;
			case eTriggerCommandType::DEACTIVATE_MIXER_PROGRAM: break;
			case eTriggerCommandType::CAST_SKILL: break;
			case eTriggerCommandType::DISPLAY_ZONE_SUMMARY: break;
			case eTriggerCommandType::SET_PHYSICS_VOLUME_EFFECT:
				HandleSetPhysicsVolume(targetEntity, argArray, command->target);
				break;
			case eTriggerCommandType::SET_PHYSICS_VOLUME_STATUS: break;
			case eTriggerCommandType::SET_MODEL_TO_BUILD: break;
			case eTriggerCommandType::SPAWN_MODEL_BRICKS: break;
			case eTriggerCommandType::ACTIVATE_SPAWNER_NETWORK: break;
			case eTriggerCommandType::DEACTIVATE_SPAWNER_NETWORK: break;
			case eTriggerCommandType::RESET_SPAWNER_NETWORK: break;
			case eTriggerCommandType::DESTROY_SPAWNER_NETWORK_OBJECTS: break;
			case eTriggerCommandType::GO_TO_WAYPOINT: break;
			case eTriggerCommandType::ACTIVATE_PHYSICS: break;
			default:
				Game::logger->LogDebug("TriggerComponent", "Event %i was not handled!", command->id);
				break;
		}
	}
}

std::vector<Entity*> TriggerComponent::GatherTargets(LUTriggers::Command* command, Entity* optionalTarget) {
	std::vector<Entity*> entities = {};

	if (command->target == "self") entities.push_back(m_Parent);
	else if (command->target == "zone") { /*TODO*/ }
	else if (command->target == "target") { /*TODO*/ }
	else if (command->target == "targetTeam") { /*TODO*/ }
	else if (command->target == "objGroup") entities = EntityManager::Instance()->GetEntitiesInGroup(command->targetName);
	else if (command->target == "allPlayers") { /*TODO*/ }
	else if (command->target == "allNPCs") { /*TODO*/ }

	if (optionalTarget) entities.push_back(optionalTarget);

	return entities;
}

void TriggerComponent::HandleSetPhysicsVolume(Entity* targetEntity, std::vector<std::string> argArray, std::string target) {
	PhantomPhysicsComponent* phanPhys = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phanPhys) return;

	phanPhys->SetPhysicsEffectActive(true);
	uint32_t effectType = 0;
	std::transform(argArray.at(0).begin(), argArray.at(0).end(), argArray.at(0).begin(), ::tolower); //Transform to lowercase
	if (argArray.at(0) == "push") effectType = 0;
	else if (argArray.at(0) == "attract") effectType = 1;
	else if (argArray.at(0) == "repulse") effectType = 2;
	else if (argArray.at(0) == "gravity") effectType = 3;
	else if (argArray.at(0) == "friction") effectType = 4;

	phanPhys->SetEffectType(effectType);
	phanPhys->SetDirectionalMultiplier(std::stof(argArray.at(1)));
	if (argArray.size() > 4) {
		NiPoint3 direction = NiPoint3::ZERO;
		GeneralUtils::TryParse<float>(argArray.at(2), direction.x);
		GeneralUtils::TryParse<float>(argArray.at(3), direction.y);
		GeneralUtils::TryParse<float>(argArray.at(4), direction.z);
		phanPhys->SetDirection(direction);
	}
	if (argArray.size() > 5) {
		uint32_t min;
		GeneralUtils::TryParse<uint32_t>(argArray.at(6), min);
		phanPhys->SetMin(min);

		uint32_t max;
		GeneralUtils::TryParse<uint32_t>(argArray.at(7), max);
		phanPhys->SetMax(max);
	}

	// TODO: why is this contruct and not serialize?
	if (target == "self") EntityManager::Instance()->ConstructEntity(m_Parent);
}

void TriggerComponent::HandleUpdateMission(Entity* targetEntity, std::vector<std::string> argArray) {
	CDMissionTasksTable* missionTasksTable = CDClientManager::Instance()->GetTable<CDMissionTasksTable>("MissionTasks");
	std::vector<CDMissionTasks> missionTasks = missionTasksTable->Query([=](CDMissionTasks entry) {
		std::string lowerTargetGroup;
		for (char character : entry.targetGroup) {
			lowerTargetGroup.push_back(std::tolower(character)); // make lowercase to ensure it works
		}

		return (lowerTargetGroup == argArray[4]);
	});

	for (const CDMissionTasks& task : missionTasks) {
		MissionComponent* missionComponent = targetEntity->GetComponent<MissionComponent>();
		if (!missionComponent) continue;

		missionComponent->ForceProgress(task.id, task.uid, std::stoi(argArray[2]));
	}
}

void TriggerComponent::HandleFireEvent(Entity* targetEntity, std::string args) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(targetEntity)) {
		script->OnFireEventServerSide(targetEntity, m_Parent, args, 0, 0, 0);
	}
}

