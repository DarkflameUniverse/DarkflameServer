#include "TriggerComponent.h"

#include "dZoneManager.h"
#include "LUTriggers.h"

#include "MissionComponent.h"
#include "PhantomPhysicsComponent.h"

#include "CDMissionTasksTable.h"


TriggerComponent::TriggerComponent(Entity* parent, const std::string triggerInfo): Component(parent) {
	m_Parent = parent;
	m_Trigger = nullptr;

	std::stringstream ss(triggerInfo);
	std::vector<std::string> tokens;
	std::string token;
	while (std::getline(ss, token, ':')) {
		tokens.push_back(token);
	}

	uint32_t sceneID = std::stoi(tokens[0]);
	uint32_t triggerID = std::stoi(tokens[1]);

	if (m_Trigger) {
		delete m_Trigger;
		m_Trigger = nullptr;
	}

	m_Trigger = dZoneManager::Instance()->GetZone()->GetTrigger(sceneID, triggerID);

	if (!m_Trigger) m_Trigger = new LUTriggers::Trigger();
}

void TriggerComponent::TriggerEvent(std::string eventID, Entity* optionalTarget) {
	if (m_Trigger != nullptr && m_Trigger->enabled) {
		for (LUTriggers::Event* triggerEvent : m_Trigger->events) {
			if (triggerEvent->eventID == eventID) {
				for (LUTriggers::Command* cmd : triggerEvent->commands) {
					HandleTriggerCommand(cmd->id, cmd->target, cmd->targetName, cmd->args, optionalTarget);
				}
			}
		}
	}
}

void TriggerComponent::HandleTriggerCommand(std::string id, std::string target, std::string targetName, std::string args, Entity* optionalTarget) {
	auto argArray = ParseArgs(args);

	// determine targets
	std::vector<Entity*> targetEntities;
	if (target == "objGroup") targetEntities = EntityManager::Instance()->GetEntitiesInGroup(targetName);
	if (target == "self") targetEntities.push_back(m_Parent);
	if (optionalTarget) targetEntities.push_back(optionalTarget);

	// if we have no targets, then we are done
	if (targetEntities.empty()) return;

	for (Entity* targetEntity : targetEntities) {
		if (!targetEntity) continue;

		if (id == "SetPhysicsVolumeEffect") {
			HandleSetPhysicsVolume(targetEntity, argArray, target);
		} else if (id == "updateMission") {
			HandleUpdateMission(targetEntity, argArray);
		} else if (id == "fireEvent") {
			HandleFireEvent(targetEntity, args);
		} else if (id == "zonePlayer") {
			// TODO:
			// zonePlayer 	[zone ID],(0 for non-instanced, 1 for instanced), (x, y, z position), (y rotation), (spawn point name)
		} else if (id == "destroyObj") {
			// TODO:
			// destroyObj 	(0 for violent, 1 for silent)
		} else if (id == "toggleTrigger") {
			// TODO:
			// toggleTrigger 	[0 to disable, 1 to enable]
		} else if (id == "resetRebuild") {
			// TODO:
			// resetRebuild 	(0 for normal reset, 1 for “failure” reset)
		} else if (id == "setPath") {
			// TODO:
			// setPath 	[new path name],(starting point index),(0 for forward, 1 for reverse)
		} else if (id == "setPickType") {
			// TODO:
			// setPickType 	[new pick type, or -1 to disable picking]
		} else if (id == "moveObject") {
			// TODO:
			// moveObject 	[x offset],[y offset],[z offset]
		} else if (id == "rotateObject") {
			// TODO:
			// rotateObject 	[x rotation],[y rotation],[z rotation]
		} else if (id == "pushObject") {
			// TODO:
			// pushObject 	[x direction],[y direction],[z direction]
		} else if (id == "repelObject") {
			// TODO:
			// repelObject 	(force multiplier)
		} else if (id == "setTimer") {
			// TODO:
			// setTimer 	[timer name],[duration in seconds]
		} else if (id == "cancelTimer") {
			// TODO:
			// cancelTimer 	[timer name]
		} else if (id == "playCinematic") {
			// TODO:
			// playCinematic 	[cinematic name],(lead-in in seconds),(“wait” to wait at end),(“unlock” to NOT lock the player controls),(“leavelocked” to leave player locked after cinematic finishes),(“hideplayer” to make player invisible during cinematic)
		} else if (id == "toggleBBB") {
			// TODO:
			// toggleBBB 	(“enter” or “exit” to force direction)
		} else if (id == "setBouncerState") {
			// TODO:
			// setBouncerState 	[“on” to activate bouncer or “off” to deactivate bouncer]
		} else if (id == "bounceAllOnBouncer") {
			// TODO:
			// bounceAllOnBouncer 	No Parameters Required
		} else if (id == "turnAroundOnPath") {
			// TODO:
			// turnAroundOnPath 	No Parameters Required
		} else if (id == "goForwardOnPath") {
			// TODO:
			// goForwardOnPath 	No Parameters Required
		} else if (id == "goBackwardOnPath") {
			// TODO:
			// goBackwardOnPath 	No Parameters Required
		} else if (id == "stopPathing") {
			// TODO:
			// stopPathing 	No Parameters Required
		} else if (id == "startPathing") {
			// TODO:
			// startPathing 	No Parameters Required
		} else if (id == "LockOrUnlockControls") {
			// TODO:
			// LockOrUnlockControls 	[“lock” to lock controls or “unlock” to unlock controls]
		} else if (id == "PlayEffect") {
			// TODO:
			// PlayEffect 	[nameID],[effectID],[effectType],[priority(optional)]
		} else if (id == "StopEffect") {
			// TODO:
			// StopEffect 	[nameID]
		} else if (id == "CastSkill") {
			// TODO:
			// CastSkill 	[skillID]
		} else if (id == "displayZoneSummary") {
			// TODO:
			// displayZoneSummary 	[1 for zone start, 0 for zone end]
		} else if (id == "SetPhysicsVolumeStatus") {
			// TODO:
			// SetPhysicsVolumeStatus 	[“On”, “Off”]
		} else if (id == "setModelToBuild") {
			// TODO:
			// setModelToBuild 	[template ID]
		} else if (id == "spawnModelBricks") {
			// TODO:
			// spawnModelBricks 	[amount, from 0 to 1],[x],[y],[z]
		} else if (id == "ActivateSpawnerNetwork") {
			// TODO:
			// ActivateSpawnerNetwork 	[Spawner Network Name]
		} else if (id == "DeactivateSpawnerNetwork") {
			// TODO:
			// DeactivateSpawnerNetwork 	[Spawner Network Name]
		} else if (id == "ResetSpawnerNetwork") {
			// TODO:
			// ResetSpawnerNetwork 	[Spawner Network Name]
		} else if (id == "DestroySpawnerNetworkObjects") {
			// TODO:
			// DestroySpawnerNetworkObjects 	[Spawner Network Name]
		} else if (id == "Go_To_Waypoint") {
			// TODO:
			// Go_To_Waypoint 	[Waypoint index],(“true” to allow direction change, otherwise “false”),(“true” to stop at waypoint, otherwise “false”)
		} else if (id == "ActivatePhysics") {
			// TODO:
			// ActivatePhysics 	“true” to activate and add to world, “false” to deactivate and remove from the world
		} else {
			Game::logger->LogDebug("TriggerComponent", "Trigger Event %s does not exist!", id.c_str());
		}
	}
}

std::vector<std::string> TriggerComponent::ParseArgs(std::string args){
	std::vector<std::string> argArray;
	std::stringstream ssData(args);
	std::string token;
	char deliminator = ',';

	while (std::getline(ssData, token, deliminator)) {
		std::string lowerToken;
		for (char character : token) {
			lowerToken.push_back(std::tolower(character)); // make lowercase to ensure it works
		}
		argArray.push_back(lowerToken);
	}
	return argArray;
}

void TriggerComponent::HandleSetPhysicsVolume(Entity* targetEntity, std::vector<std::string> argArray, std::string target) {
	PhantomPhysicsComponent* phanPhys = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phanPhys) return;

	phanPhys->SetPhysicsEffectActive(true);
	uint32_t effectType = 0;
	if (argArray[0] == "push") effectType = 0;
	else if (argArray[0] == "attract") effectType = 1;
	else if (argArray[0] == "repulse") effectType = 2;
	else if (argArray[0] == "gravity") effectType = 3;
	else if (argArray[0] == "friction") effectType = 4;

	phanPhys->SetEffectType(effectType);
	phanPhys->SetDirectionalMultiplier(std::stof(argArray[1]));
	if (argArray.size() > 4) {
		NiPoint3 direction = NiPoint3::ZERO;
		GeneralUtils::TryParse<float>(argArray[2], direction.x);
		GeneralUtils::TryParse<float>(argArray[3], direction.y);
		GeneralUtils::TryParse<float>(argArray[4], direction.z);
		phanPhys->SetDirection(direction);
	}
	if (argArray.size() > 5) {
		phanPhys->SetMin(std::stoi(argArray[6]));
		phanPhys->SetMax(std::stoi(argArray[7]));
	}

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

