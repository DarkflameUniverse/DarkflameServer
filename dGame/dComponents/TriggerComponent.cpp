#include "TriggerComponent.h"
#include "dZoneManager.h"
#include "TeamManager.h"
#include "eTriggerCommandType.h"
#include "eMissionTaskType.h"

#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "MissionComponent.h"
#include "PhantomPhysicsComponent.h"
#include "Player.h"
#include "RebuildComponent.h"
#include "SkillComponent.h"


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
			case eTriggerCommandType::SET_TIMER: break;
			case eTriggerCommandType::CANCEL_TIMER: break;
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
	else if (command->target == "target") if (optionalTarget) entities.push_back(optionalTarget);
	else if (command->target == "targetTeam") {
		auto* team = TeamManager::Instance()->GetTeam(optionalTarget->GetObjectID());
		for (const auto memberId : team->members) {
			auto* member = EntityManager::Instance()->GetEntity(memberId);
			if (member) entities.push_back(member);
		}
	} else if (command->target == "objGroup") entities = EntityManager::Instance()->GetEntitiesInGroup(command->targetName);
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
	if (!triggerComponent) return;
	triggerComponent->SetTriggerEnabled(args == "1");
}

void TriggerComponent::HandleResetRebuild(Entity* targetEntity, std::string args){
	auto* rebuildComponent = targetEntity->GetComponent<RebuildComponent>();
	if (!rebuildComponent) return;
	rebuildComponent->ResetRebuild(args == "1");
}

void TriggerComponent::HandleMoveObject(Entity* targetEntity, std::vector<std::string> argArray){
	auto position = targetEntity->GetPosition();

	NiPoint3 offset = NiPoint3::ZERO;
	GeneralUtils::TryParse<float>(argArray.at(0), offset.x);
	GeneralUtils::TryParse<float>(argArray.at(1), offset.y);
	GeneralUtils::TryParse<float>(argArray.at(2), offset.z);

	position.x += offset.x;
	position.y += offset.y;
	position.z += offset.z;
	targetEntity->SetPosition(position);
}

void TriggerComponent::HandleRotateObject(Entity* targetEntity, std::vector<std::string> argArray){
	NiPoint3 vector = NiPoint3::ZERO;
	GeneralUtils::TryParse<float>(argArray.at(0), vector.x);
	GeneralUtils::TryParse<float>(argArray.at(1), vector.y);
	GeneralUtils::TryParse<float>(argArray.at(2), vector.z);
	NiQuaternion rotation = NiQuaternion::FromEulerAngles(vector);
	targetEntity->SetRotation(rotation);
}

void TriggerComponent::HandlePushObject(Entity* targetEntity, std::vector<std::string> argArray){
	auto* phantomPhysicsComponent = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(0); // push
	phantomPhysicsComponent->SetDirectionalMultiplier(1);
	NiPoint3 direction = NiPoint3::ZERO;
	GeneralUtils::TryParse<float>(argArray.at(0), direction.x);
	GeneralUtils::TryParse<float>(argArray.at(1), direction.y);
	GeneralUtils::TryParse<float>(argArray.at(2), direction.z);
	phantomPhysicsComponent->SetDirection(direction);

	EntityManager::Instance()->SerializeEntity(m_Parent);
}


void TriggerComponent::HandleRepelObject(Entity* targetEntity, std::string args){
	auto* phantomPhysicsComponent = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;
	float forceMultiplier;
	GeneralUtils::TryParse<float>(args, forceMultiplier);
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(1); // repel/repulse
	phantomPhysicsComponent->SetDirectionalMultiplier(forceMultiplier);

	auto triggerPos = m_Parent->GetPosition();
	auto targetPos = targetEntity->GetPosition();

	// normalize the vectors to get the direction
	auto delta = targetPos - triggerPos;
	auto abs = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
	NiPoint3 direction = NiPoint3::ZERO;
	direction.x = delta.x / abs;
	direction.y = delta.y / abs;
	direction.z = delta.z / abs;

	phantomPhysicsComponent->SetDirection(direction);

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void TriggerComponent::HandlePlayCinematic(Entity* targetEntity, std::vector<std::string> argArray) {
	// [cinematic name],(lead-in in seconds),(“wait” to wait at end),(“unlock” to NOT lock the player controls),(“leavelocked” to leave player locked after cinematic finishes),(“hideplayer” to make player invisible during cinematic)
	float leadIn = -1.0;
	if (argArray.size() >= 2) GeneralUtils::TryParse<float>(argArray.at(1), leadIn);
	int wait = 0;
	if (argArray.size() >= 3 && argArray.at(2) == "wait") wait = 1;
	bool unlock;
	if (argArray.size() >= 4 && argArray.at(3) == "unlock") unlock = false;
	bool leaveLocked = false;
	if (argArray.size() >= 5 && argArray.at(4) == "leavelocked") leaveLocked = true;
	bool hidePlayer = false;
	if (argArray.size() >= 6 && argArray.at(5) == "hideplayer") hidePlayer = true;
	GameMessages::SendPlayCinematic(targetEntity->GetObjectID(), GeneralUtils::UTF8ToUTF16(argArray.at(0)), targetEntity->GetSystemAddress(), true, true, false, false, 0, hidePlayer, leadIn, leaveLocked, unlock);
}

void TriggerComponent::HandleToggleBBB(Entity* targetEntity, std::string args) {
	auto* character = targetEntity->GetCharacter();
	if (!character) return;
	bool buildMode = false;
	if (args == "enter") buildMode = true;
	else if (args == "exit") buildMode = false;
	else buildMode = !(character->GetBuildMode());
	character->SetBuildMode(buildMode);
}

void TriggerComponent::HandleUpdateMission(Entity* targetEntity, std::vector<std::string> argArray) {
	// there are only explore tasks used
	// If others need to be implemented for modding
	// then we need a good way to convert this from a string to that enum
	if (argArray.at(0) != "exploretask") return;
	MissionComponent* missionComponent = targetEntity->GetComponent<MissionComponent>();
	if (!missionComponent) return;
	missionComponent->Progress(eMissionTaskType::EXPLORE, 0, 0, argArray.at(4));
}

void TriggerComponent::HandlePlayEffect(Entity* targetEntity, std::vector<std::string> argArray) {
	// [nameID],[effectID],[effectType],[priority(optional)]
	int32_t effectID = 0;
	if (!GeneralUtils::TryParse<int32_t>(argArray.at(1), effectID)) return;
	std::u16string effectType = GeneralUtils::UTF8ToUTF16(argArray.at(2));
	float priority = 1;
	GeneralUtils::TryParse<float>(argArray.at(3), priority);
	GameMessages::SendPlayFXEffect(targetEntity, effectID, effectType, argArray.at(0), LWOOBJID_EMPTY, priority, 1, true);
}

void TriggerComponent::HandleCastSkill(Entity* targetEntity, std::string args){
	auto* skillComponent = targetEntity->GetComponent<SkillComponent>();
	if (!skillComponent) return;
	uint32_t skillId;
	GeneralUtils::TryParse<uint32_t>(args, skillId);
	skillComponent->CastSkill(skillId, targetEntity->GetObjectID());
}

void TriggerComponent::HandleSetPhysicsVolumeEffect(Entity* targetEntity, std::vector<std::string> argArray) {
	auto* phantomPhysicsComponent = targetEntity->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;

	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	uint32_t effectType = 0;
	std::transform(argArray.at(0).begin(), argArray.at(0).end(), argArray.at(0).begin(), ::tolower); //Transform to lowercase
	if (argArray.at(0) == "push") effectType = 0;
	else if (argArray.at(0) == "attract") effectType = 1;
	else if (argArray.at(0) == "repulse") effectType = 2;
	else if (argArray.at(0) == "gravity") effectType = 3;
	else if (argArray.at(0) == "friction") effectType = 4;

	phantomPhysicsComponent->SetEffectType(effectType);
	phantomPhysicsComponent->SetDirectionalMultiplier(std::stof(argArray.at(1)));
	if (argArray.size() > 4) {
		NiPoint3 direction = NiPoint3::ZERO;
		GeneralUtils::TryParse<float>(argArray.at(2), direction.x);
		GeneralUtils::TryParse<float>(argArray.at(3), direction.y);
		GeneralUtils::TryParse<float>(argArray.at(4), direction.z);
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

	EntityManager::Instance()->SerializeEntity(targetEntity);
}

void TriggerComponent::HandleSetPhysicsVolumeStatus(Entity* targetEntity, std::string args) {
	PhantomPhysicsComponent* phanPhys = targetEntity->GetComponent<PhantomPhysicsComponent>();
	if (!phanPhys) return;
	phanPhys->SetPhysicsEffectActive(args == "On");
	EntityManager::Instance()->SerializeEntity(targetEntity);
}

void TriggerComponent::HandleActivateSpawnerNetwork(std::string args){
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(args)) {
		spawner->Activate();
	}
}

void TriggerComponent::HandleDeactivateSpawnerNetwork(std::string args){
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(args)) {
		spawner->Deactivate();
	}
}

void TriggerComponent::HandleResetSpawnerNetwork(std::string args){
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(args)) {
		spawner->Reset();
	}
}

void TriggerComponent::HandleDestroySpawnerNetworkObjects(std::string args){
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(args)) {
		for (auto* node : spawner->m_Info.nodes) {
			for (const auto& element : node->entities) {
				auto* entity = EntityManager::Instance()->GetEntity(element);
				if (entity == nullptr) continue;
				entity->Kill();
			}
			node->entities.clear();
		}
	}
}
