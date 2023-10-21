#include "TriggerComponent.h"
#include "dZoneManager.h"
#include "TeamManager.h"
#include "eTriggerCommandType.h"
#include "eMissionTaskType.h"
#include "ePhysicsEffectType.h"

#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "MissionComponent.h"
#include "PhantomPhysicsComponent.h"
#include "Player.h"
#include "RebuildComponent.h"
#include "SkillComponent.h"
#include "eEndBehavior.h"


TriggerComponent::TriggerComponent(Entity* parent, const std::string triggerInfo): Component(parent) {
	m_Parent = parent;
	m_Trigger = nullptr;

	std::vector<std::string> tokens = GeneralUtils::SplitString(triggerInfo, ':');

	uint32_t sceneID;
	GeneralUtils::TryParse<uint32_t>(tokens.at(0), sceneID);
	uint32_t triggerID;
	GeneralUtils::TryParse<uint32_t>(tokens.at(1), triggerID);

	m_Trigger = Game::zoneManager->GetZone()->GetTrigger(sceneID, triggerID);

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
			case eTriggerCommandType::DESTROY_OBJ:
				HandleDestroyObject(targetEntity, command->args);
				break;
			case eTriggerCommandType::TOGGLE_TRIGGER:
				HandleToggleTrigger(targetEntity, command->args);
				break;
			case eTriggerCommandType::RESET_REBUILD:
				HandleResetRebuild(targetEntity, command->args);
				break;
			case eTriggerCommandType::SET_PATH: break;
			case eTriggerCommandType::SET_PICK_TYPE: break;
			case eTriggerCommandType::MOVE_OBJECT:
				HandleMoveObject(targetEntity, argArray);
				break;
			case eTriggerCommandType::ROTATE_OBJECT:
				HandleRotateObject(targetEntity, argArray);
				break;
			case eTriggerCommandType::PUSH_OBJECT:
				HandlePushObject(targetEntity, argArray);
				break;
			case eTriggerCommandType::REPEL_OBJECT:
				HandleRepelObject(targetEntity, command->args);
				break;
			case eTriggerCommandType::SET_TIMER:
				HandleSetTimer(targetEntity, argArray);
				break;
			case eTriggerCommandType::CANCEL_TIMER:
				HandleCancelTimer(targetEntity, command->args);
				break;
			case eTriggerCommandType::PLAY_CINEMATIC:
				HandlePlayCinematic(targetEntity, argArray);
				break;
			case eTriggerCommandType::TOGGLE_BBB:
				HandleToggleBBB(targetEntity, command->args);
				break;
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
			case eTriggerCommandType::PLAY_EFFECT:
				HandlePlayEffect(targetEntity, argArray);
				break;
			case eTriggerCommandType::STOP_EFFECT:
				GameMessages::SendStopFXEffect(targetEntity, true, command->args);
				break;
			case eTriggerCommandType::CAST_SKILL:
				HandleCastSkill(targetEntity, command->args);
				break;
			case eTriggerCommandType::DISPLAY_ZONE_SUMMARY:
				GameMessages::SendDisplayZoneSummary(targetEntity->GetObjectID(), targetEntity->GetSystemAddress(), false, command->args == "1", m_Parent->GetObjectID());
				break;
			case eTriggerCommandType::SET_PHYSICS_VOLUME_EFFECT:
				HandleSetPhysicsVolumeEffect(targetEntity, argArray);
				break;
			case eTriggerCommandType::SET_PHYSICS_VOLUME_STATUS:
				HandleSetPhysicsVolumeStatus(targetEntity, command->args);
				break;
			case eTriggerCommandType::SET_MODEL_TO_BUILD: break;
			case eTriggerCommandType::SPAWN_MODEL_BRICKS: break;
			case eTriggerCommandType::ACTIVATE_SPAWNER_NETWORK:
				HandleActivateSpawnerNetwork(command->args);
				break;
			case eTriggerCommandType::DEACTIVATE_SPAWNER_NETWORK:
				HandleDeactivateSpawnerNetwork(command->args);
				break;
			case eTriggerCommandType::RESET_SPAWNER_NETWORK:
				HandleResetSpawnerNetwork(command->args);
				break;
			case eTriggerCommandType::DESTROY_SPAWNER_NETWORK_OBJECTS:
				HandleDestroySpawnerNetworkObjects(command->args);
				break;
			case eTriggerCommandType::GO_TO_WAYPOINT: break;
			case eTriggerCommandType::ACTIVATE_PHYSICS:
				HandleActivatePhysics(targetEntity, command->args);
				break;
			// DEPRECATED BLOCK START
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
			// DEPRECATED BLOCK END
			default:
				LOG_DEBUG("Event %i was not handled!", command->id);
				break;
		}
	}
}

std::vector<Entity*> TriggerComponent::GatherTargets(LUTriggers::Command* command, Entity* optionalTarget) {
	std::vector<Entity*> entities = {};

	if (command->target == "self") entities.push_back(m_Parent);
	else if (command->target == "zone") { /*TODO*/ }
	else if (command->target == "target" && optionalTarget) entities.push_back(optionalTarget);
	else if (command->target == "targetTeam" && optionalTarget) {
		auto* team = TeamManager::Instance()->GetTeam(optionalTarget->GetObjectID());
		for (const auto memberId : team->members) {
			auto* member = Game::entityManager->GetEntity(memberId);
			if (member) entities.push_back(member);
		}
	} else if (command->target == "objGroup") entities = Game::entityManager->GetEntitiesInGroup(command->targetName);
	else if (command->target == "allPlayers") {
		for (auto* player : Player::GetAllPlayers()) {
			entities.push_back(player);
		}
	} else if (command->target == "allNPCs") { /*UNUSED*/ }

	return entities;
}

void TriggerComponent::HandleFireEvent(Entity* targetEntity, std::string args) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(targetEntity)) {
		script->OnFireEventServerSide(targetEntity, m_Parent, args, 0, 0, 0);
	}
}

void TriggerComponent::HandleDestroyObject(Entity* targetEntity, std::string args){
	uint32_t killType;
	GeneralUtils::TryParse<uint32_t>(args, killType);
	targetEntity->Smash(m_Parent->GetObjectID(), static_cast<eKillType>(killType));
}

void TriggerComponent::HandleToggleTrigger(Entity* targetEntity, std::string args){
	auto* triggerComponent = targetEntity->GetComponent<TriggerComponent>();
	if (!triggerComponent) {
		LOG_DEBUG("Trigger component not found!");
		return;
	}
	triggerComponent->SetTriggerEnabled(args == "1");
}

void TriggerComponent::HandleResetRebuild(Entity* targetEntity, std::string args){
	auto* rebuildComponent = targetEntity->GetComponent<RebuildComponent>();
	if (!rebuildComponent) {
		LOG_DEBUG("Rebuild component not found!");
		return;
	}
	rebuildComponent->ResetRebuild(args == "1");
}

void TriggerComponent::HandleMoveObject(Entity* targetEntity, std::vector<std::string> argArray){
	if (argArray.size() <= 2) return;

	auto position = targetEntity->GetPosition();
	NiPoint3 offset = NiPoint3::ZERO;
	GeneralUtils::TryParse(argArray.at(0), argArray.at(1), argArray.at(2), offset);

	position += offset;
	targetEntity->SetPosition(position);
}

void TriggerComponent::HandleRotateObject(Entity* targetEntity, std::vector<std::string> argArray){
	if (argArray.size() <= 2) return;

	NiPoint3 vector = NiPoint3::ZERO;
	GeneralUtils::TryParse(argArray.at(0), argArray.at(1), argArray.at(2), vector);

	NiQuaternion rotation = NiQuaternion::FromEulerAngles(vector);
	targetEntity->SetRotation(rotation);
}

void TriggerComponent::HandlePushObject(Entity* targetEntity, std::vector<std::string> argArray){
	if (argArray.size() < 3) return;

	auto* phantomPhysicsComponent = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) {
		LOG_DEBUG("Phantom Physics component not found!");
		return;
	}
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(ePhysicsEffectType::PUSH);
	phantomPhysicsComponent->SetDirectionalMultiplier(1);
	NiPoint3 direction = NiPoint3::ZERO;
	GeneralUtils::TryParse(argArray.at(0), argArray.at(1), argArray.at(2), direction);
	phantomPhysicsComponent->SetDirection(direction);

	Game::entityManager->SerializeEntity(m_Parent);
}


void TriggerComponent::HandleRepelObject(Entity* targetEntity, std::string args){
	auto* phantomPhysicsComponent = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) {
		LOG_DEBUG("Phantom Physics component not found!");
		return;
	}
	float forceMultiplier;
	GeneralUtils::TryParse<float>(args, forceMultiplier);
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(ePhysicsEffectType::REPULSE);
	phantomPhysicsComponent->SetDirectionalMultiplier(forceMultiplier);

	auto triggerPos = m_Parent->GetPosition();
	auto targetPos = targetEntity->GetPosition();

	// normalize the vectors to get the direction
	auto delta = targetPos - triggerPos;
	auto length = delta.Length();
	NiPoint3 direction = delta / length;
	phantomPhysicsComponent->SetDirection(direction);

	Game::entityManager->SerializeEntity(m_Parent);
}

void TriggerComponent::HandleSetTimer(Entity* targetEntity, std::vector<std::string> argArray){
	if (argArray.size() != 2) {
		LOG_DEBUG("Not ehought variables!");
		return;
	}
	float time = 0.0;
	GeneralUtils::TryParse<float>(argArray.at(1), time);
	m_Parent->AddTimer(argArray.at(0), time);
}

void TriggerComponent::HandleCancelTimer(Entity* targetEntity, std::string args){
	m_Parent->CancelTimer(args);
}

void TriggerComponent::HandlePlayCinematic(Entity* targetEntity, std::vector<std::string> argArray) {
	float leadIn = -1.0;
	auto wait = eEndBehavior::RETURN;
	bool unlock = true;
	bool leaveLocked = false;
	bool hidePlayer = false;

	if (argArray.size() >= 2) {
		GeneralUtils::TryParse<float>(argArray.at(1), leadIn);
		if (argArray.size() >= 3 && argArray.at(2) == "wait") {
			wait = eEndBehavior::WAIT;
			if (argArray.size() >= 4 && argArray.at(3) == "unlock") {
				unlock = false;
				if (argArray.size() >= 5 && argArray.at(4) == "leavelocked") {
					leaveLocked = true;
					if (argArray.size() >= 6 && argArray.at(5) == "hideplayer") {
						hidePlayer = true;
					}
				}
			}
		}
	}

	GameMessages::SendPlayCinematic(targetEntity->GetObjectID(), GeneralUtils::UTF8ToUTF16(argArray.at(0)), targetEntity->GetSystemAddress(), true, true, false, false, wait, hidePlayer, leadIn, leaveLocked, unlock);
}

void TriggerComponent::HandleToggleBBB(Entity* targetEntity, std::string args) {
	auto* character = targetEntity->GetCharacter();
	if (!character) {
		LOG_DEBUG("Character was not found!");
		return;
	}
	bool buildMode = !(character->GetBuildMode());
	if (args == "enter") buildMode = true;
	else if (args == "exit") buildMode = false;
	character->SetBuildMode(buildMode);
}

void TriggerComponent::HandleUpdateMission(Entity* targetEntity, std::vector<std::string> argArray) {
	// there are only explore tasks used
	// If others need to be implemented for modding
	// then we need a good way to convert this from a string to that enum
	if (argArray.at(0) != "exploretask") return;
	MissionComponent* missionComponent = targetEntity->GetComponent<MissionComponent>();
	if (!missionComponent){
		LOG_DEBUG("Mission component not found!");
		return;
	}
	missionComponent->Progress(eMissionTaskType::EXPLORE, 0, 0, argArray.at(4));
}

void TriggerComponent::HandlePlayEffect(Entity* targetEntity, std::vector<std::string> argArray) {
	if (argArray.size() < 3) return;
	int32_t effectID = 0;
	if (!GeneralUtils::TryParse<int32_t>(argArray.at(1), effectID)) return;
	std::u16string effectType = GeneralUtils::UTF8ToUTF16(argArray.at(2));
	float priority = 1;
	if (argArray.size() == 4) GeneralUtils::TryParse<float>(argArray.at(3), priority);
	GameMessages::SendPlayFXEffect(targetEntity, effectID, effectType, argArray.at(0), LWOOBJID_EMPTY, priority);
}

void TriggerComponent::HandleCastSkill(Entity* targetEntity, std::string args){
	auto* skillComponent = targetEntity->GetComponent<SkillComponent>();
	if (!skillComponent) {
		LOG_DEBUG("Skill component not found!");
		return;
	}
	uint32_t skillId;
	GeneralUtils::TryParse<uint32_t>(args, skillId);
	skillComponent->CastSkill(skillId, targetEntity->GetObjectID());
}

void TriggerComponent::HandleSetPhysicsVolumeEffect(Entity* targetEntity, std::vector<std::string> argArray) {
	auto* phantomPhysicsComponent = targetEntity->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) {
		LOG_DEBUG("Phantom Physics component not found!");
		return;
	}
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	ePhysicsEffectType effectType = ePhysicsEffectType::PUSH;
	std::transform(argArray.at(0).begin(), argArray.at(0).end(), argArray.at(0).begin(), ::tolower); //Transform to lowercase
	if (argArray.at(0) == "push") effectType = ePhysicsEffectType::PUSH;
	else if (argArray.at(0) == "attract") effectType = ePhysicsEffectType::ATTRACT;
	else if (argArray.at(0) == "repulse") effectType = ePhysicsEffectType::REPULSE;
	else if (argArray.at(0) == "gravity") effectType = ePhysicsEffectType::GRAVITY_SCALE;
	else if (argArray.at(0) == "friction") effectType = ePhysicsEffectType::FRICTION;

	phantomPhysicsComponent->SetEffectType(effectType);
	phantomPhysicsComponent->SetDirectionalMultiplier(std::stof(argArray.at(1)));
	if (argArray.size() > 4) {
		NiPoint3 direction = NiPoint3::ZERO;
		GeneralUtils::TryParse(argArray.at(2), argArray.at(3), argArray.at(4), direction);
		phantomPhysicsComponent->SetDirection(direction);
	}
	if (argArray.size() > 5) {
		uint32_t min;
		GeneralUtils::TryParse<uint32_t>(argArray.at(6), min);
		phantomPhysicsComponent->SetMin(min);

		uint32_t max;
		GeneralUtils::TryParse<uint32_t>(argArray.at(7), max);
		phantomPhysicsComponent->SetMax(max);
	}

	Game::entityManager->SerializeEntity(targetEntity);
}

void TriggerComponent::HandleSetPhysicsVolumeStatus(Entity* targetEntity, std::string args) {
	auto* phantomPhysicsComponent = targetEntity->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) {
		LOG_DEBUG("Phantom Physics component not found!");
		return;
	}
	phantomPhysicsComponent->SetPhysicsEffectActive(args == "On");
	Game::entityManager->SerializeEntity(targetEntity);
}

void TriggerComponent::HandleActivateSpawnerNetwork(std::string args){
	for (auto* spawner : Game::zoneManager->GetSpawnersByName(args)) {
		if (spawner) spawner->Activate();
	}
}

void TriggerComponent::HandleDeactivateSpawnerNetwork(std::string args){
	for (auto* spawner : Game::zoneManager->GetSpawnersByName(args)) {
		if (spawner) spawner->Deactivate();
	}
}

void TriggerComponent::HandleResetSpawnerNetwork(std::string args){
	for (auto* spawner : Game::zoneManager->GetSpawnersByName(args)) {
		if (spawner) spawner->Reset();
	}
}

void TriggerComponent::HandleDestroySpawnerNetworkObjects(std::string args){
	for (auto* spawner : Game::zoneManager->GetSpawnersByName(args)) {
		if (spawner) spawner->DestroyAllEntities();
	}
}

void TriggerComponent::HandleActivatePhysics(Entity* targetEntity, std::string args) {
	if (args == "true") {
		// TODO add physics entity if there isn't one
	} else if (args == "false"){
		// TODO remove Phsyics entity if there is one
	} else {
		LOG_DEBUG("Invalid argument for ActivatePhysics Trigger: %s", args.c_str());
	}
}
