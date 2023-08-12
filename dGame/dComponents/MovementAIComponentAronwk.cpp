#ifndef MOVEMENTAICOMPONENT_H
#include "MovementAIComponent.h"
#endif
#include "eWaypointCommandType.h"
#include "RenderComponent.h"
#include "SkillComponent.h"

void MovementAIComponent::HandleWaypointArrived() {
	if (!m_Path) return;
	if (m_Path->pathWaypoints.at(m_CurrentPathWaypointIndex).commands.empty()) return;
	for(auto [command, data] : m_Path->pathWaypoints.at(m_CurrentPathWaypointIndex).commands){
		switch(command){
			case eWaypointCommandType::STOP:
				Stop();
				break;
			case eWaypointCommandType::GROUP_EMOTE:
				HandleWaypointCommandGroupEmote(data);
				break;
			case eWaypointCommandType::SET_VARIABLE:
				HandleWaypointCommandSetVariable(data);
				break;
			case eWaypointCommandType::CAST_SKILL:
				HandleWaypointCommandCastSkill(data);
				break;
			case eWaypointCommandType::EQUIP_INVENTORY:
				HandleWaypointCommandEquipInventory(data);
				break;
			case eWaypointCommandType::UNEQUIP_INVENTORY:
				HandleWaypointCommandUnequipInventory(data);
				break;
			case eWaypointCommandType::DELAY:
				HandleWaypointCommandDelay(data);
				break;
			case eWaypointCommandType::EMOTE:
				HandleWaypointCommandEmote(data);
				break;
			case eWaypointCommandType::TELEPORT:
				HandleWaypointCommandTeleport(data);
				break;
			case eWaypointCommandType::PATH_SPEED:
				HandleWaypointCommandPathSpeed(data);
				break;
			case eWaypointCommandType::REMOVE_NPC:
				HandleWaypointCommandRemoveNPC(data);
				break;
			case eWaypointCommandType::CHANGE_WAYPOINT:
				HandleWaypointCommandChangeWaypoint(data);
				break;
			case eWaypointCommandType::KILL_SELF:
				m_Parent->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
				break;
			case eWaypointCommandType::DELETE_SELF:
				m_Parent->Kill();
				break;
			case eWaypointCommandType::SPAWN_OBJECT:
				HandleWaypointCommandSpawnObject(data);
				break;
			case eWaypointCommandType::PLAY_SOUND:
				GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, data);
				break;
			case eWaypointCommandType::BOUNCE:
				Game::logger->LogDebug("MovementAIComponent", "Unusable Command %i", command);
				break;
			case eWaypointCommandType::INVALID:
			default:
				Game::logger->LogDebug("MovementAIComponent", "Got invalid waypoint command %i", command);
				break;
		}
	}
}

void MovementAIComponent::HandleWaypointCommandGroupEmote(std::string data) {
	const auto& split = GeneralUtils::SplitString(data, ';');
	if (split.size() != 2) return;
	const auto& entities = Game::entityManager->GetEntitiesInGroup(split[0]);
	for (auto& entity: entities){
		RenderComponent::PlayAnimation(entity, split[1]);
	}
	// delay for animation time
}
void MovementAIComponent::HandleWaypointCommandSetVariable(std::string data) {
	const auto& split = GeneralUtils::SplitString(data, '=');
	m_Parent->SetNetworkVar(GeneralUtils::ASCIIToUTF16(split[0]), split[1]);
}
void MovementAIComponent::HandleWaypointCommandCastSkill(std::string data) {
	if (data.empty()) return;
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();
	if (!skillComponent) {
		Game::logger->LogDebug("MovementAIComponent::HandleWaypointArrived", "Skill component not found!");
		return;
	}
	uint32_t skillId = 0;
	GeneralUtils::TryParse<uint32_t>(data, skillId);
	if (skillId != 0) skillComponent->CastSkill(skillId);
	// add some delay??
}
void MovementAIComponent::HandleWaypointCommandEquipInventory(std::string data) {
	// equip item via ID (not lot???)
}
void MovementAIComponent::HandleWaypointCommandUnequipInventory(std::string data) {
	// unequip item via ID (not lot??)
}
void MovementAIComponent::HandleWaypointCommandDelay(std::string data) {
	Pause();
	std::remove_if(data.begin(), data.end(), ::isspace);
	// delay for time
}
void MovementAIComponent::HandleWaypointCommandEmote(std::string data) {
	// pause for animation time
	auto delay = RenderComponent::PlayAnimation(m_Parent, data);
}
void MovementAIComponent::HandleWaypointCommandTeleport(std::string data) {
	auto posString = GeneralUtils::SplitString(data, ',');
	if (posString.size() == 0) return;
	auto newPos = NiPoint3();
	if (posString.size() == 1) GeneralUtils::TryParse<float>(posString.at(0), newPos.x);
	if (posString.size() == 2) GeneralUtils::TryParse<float>(posString.at(1), newPos.y);
	if (posString.size() == 3) GeneralUtils::TryParse<float>(posString.at(2), newPos.z);
	GameMessages::SendTeleport(m_Parent->GetObjectID(), newPos, NiQuaternion::IDENTITY, UNASSIGNED_SYSTEM_ADDRESS);
}
void MovementAIComponent::HandleWaypointCommandPathSpeed(std::string data) {
	float speed = 0.0;
	GeneralUtils::TryParse<float>(data, speed);
	SetMaxSpeed(speed);
}
void MovementAIComponent::HandleWaypointCommandRemoveNPC(std::string data) {
	// get objects in proximity
	// KillOBJS ???
}
void MovementAIComponent::HandleWaypointCommandChangeWaypoint(std::string data) {
	std::string path_string = "";
	int32_t index = 0;
	// sometimes there's a path and what waypoint to start, which are comma separated
	if (data.find(",") != std::string::npos){
		auto datas = GeneralUtils::SplitString(data, ',');
		path_string = datas[0];
		index = stoi(datas[1]);
	} else path_string = data;

	if (path_string != "") {
		SetupPath(path_string);
		SetCurrentPathWaypointIndex(index);
		SetNextPathWaypointIndex(index);
	}
}
void MovementAIComponent::HandleWaypointCommandSpawnObject(std::string data) {
	// just do it
}
