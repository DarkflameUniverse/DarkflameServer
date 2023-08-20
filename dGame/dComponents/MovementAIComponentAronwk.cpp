#ifndef MOVEMENTAICOMPONENT_H
#include "MovementAIComponent.h"
#endif
#include "eWaypointCommandType.h"
#include "RenderComponent.h"
#include "SkillComponent.h"
#include "InventoryComponent.h"
#include "Zone.h"
#include "EntityInfo.h"
#include "ProximityMonitorComponent.h"
#include "DestroyableComponent.h"

void MovementAIComponent::HandleWaypointArrived(uint32_t commandIndex) {
	if (!m_Path){
		if(IsPaused()) Resume();
		return;
	}
	if (commandIndex >= m_Path->pathWaypoints.at(m_CurrentPathWaypointIndex).commands.size()){
		if(IsPaused()) Resume();
		return;
	}
	if(!IsPaused()) Pause();
	const auto& data = m_Path->pathWaypoints.at(m_CurrentPathWaypointIndex).commands.at(commandIndex).data;
	const auto& command = m_Path->pathWaypoints.at(m_CurrentPathWaypointIndex).commands.at(commandIndex).command;
	float delay = 0.0f;
	switch(command){
		case eWaypointCommandType::STOP:
			Stop();
			break;
		case eWaypointCommandType::GROUP_EMOTE:
			delay = HandleWaypointCommandGroupEmote(data);
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
			delay = HandleWaypointCommandDelay(data);
			break;
		case eWaypointCommandType::EMOTE:
			delay = RenderComponent::PlayAnimation(m_Parent, data);
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
			Game::logger->LogDebug("MovementAIComponent", "Unable to process bounce waypoint command server side!");
			break;
		case eWaypointCommandType::INVALID:
		default:
			Game::logger->LogDebug("MovementAIComponent", "Got invalid waypoint command %i", command);
			break;
	}

	m_Parent->AddCallbackTimer(delay, [this, commandIndex](){
		this->HandleWaypointArrived(commandIndex + 1);
		}
	);
}

float MovementAIComponent::HandleWaypointCommandGroupEmote(const std::string& data) {
	const auto& split = GeneralUtils::SplitString(data, ';');
	if (split.size() != 2) return 0.0f;
	const auto& entities = Game::entityManager->GetEntitiesInGroup(split.at(0));
	float delay = 0.0f;
	for (auto& entity: entities){
		delay = RenderComponent::PlayAnimation(entity, split.at(1));
	}
	return delay;
}

void MovementAIComponent::HandleWaypointCommandSetVariable(const std::string& data) {
	const auto& split = GeneralUtils::SplitString(data, ',');
	m_Parent->SetNetworkVar(GeneralUtils::ASCIIToUTF16(split.at(0)), split.at(1));
}

void MovementAIComponent::HandleWaypointCommandCastSkill(const std::string& data) {
	if (data.empty()) return;
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();
	if (!skillComponent) {
		Game::logger->LogDebug("MovementAIComponent::HandleWaypointCommandCastSkill", "Skill component not found!");
		return;
	}
	uint32_t skillId = 0;
	if (!GeneralUtils::TryParse<uint32_t>(data, skillId)) return;
	if (skillId != 0) skillComponent->CastSkill(skillId);
	return;
}

void MovementAIComponent::HandleWaypointCommandEquipInventory(const std::string& data) {
	if (data.empty()) return;
	auto* inventoryComponent = m_Parent->GetComponent<InventoryComponent>();
	if (!inventoryComponent) {
		Game::logger->LogDebug("MovementAIComponent::HandleWaypointCommandEquipInventory", "Inventory component not found!");
		return;
	}
	// the client says use slot 0 of items
	const auto inventory = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	if (!inventory) return;
	const auto slots = inventory->GetSlots();
	const auto item = slots.find(0);
	if (item != slots.end()) inventoryComponent->EquipItem(item->second);
}

void MovementAIComponent::HandleWaypointCommandUnequipInventory(const std::string& data) {
	if (data.empty()) return;
	auto* inventoryComponent = m_Parent->GetComponent<InventoryComponent>();
	if (!inventoryComponent) {
		Game::logger->LogDebug("MovementAIComponent::HandleWaypointCommandEquipInventory", "Inventory component not found!");
		return;
	}
	// the client says use slot 0 of items
	const auto inventory = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	if (!inventory) return;
	const auto slots = inventory->GetSlots();
	const auto item = slots.find(0);
	if (item != slots.end()) inventoryComponent->UnEquipItem(item->second);
}

float MovementAIComponent::HandleWaypointCommandDelay(const std::string& data) {
	float delay = 0.0f;
	std::string delayString = data;
	if (!GeneralUtils::TryParse<float>(delayString, delay)){
		Game::logger->LogDebug("MovementAIComponentAronwk", "Failed to parse delay %s", data.c_str());
	}
	return delay;
}

void MovementAIComponent::HandleWaypointCommandTeleport(const std::string& data) {
	auto posString = GeneralUtils::SplitString(data, ',');
	if (posString.size() == 0) return;
	auto newPos = NiPoint3();
	if (posString.size() == 1 && !GeneralUtils::TryParse<float>(posString.at(0), newPos.x)) return;
	if (posString.size() == 2 && !GeneralUtils::TryParse<float>(posString.at(1), newPos.y)) return;
	if (posString.size() == 3 && !GeneralUtils::TryParse<float>(posString.at(2), newPos.z)) return;
	GameMessages::SendTeleport(m_Parent->GetObjectID(), newPos, NiQuaternion::IDENTITY, UNASSIGNED_SYSTEM_ADDRESS);
}

void MovementAIComponent::HandleWaypointCommandPathSpeed(const std::string& data) {
	float speed = 0.0;
	if (!GeneralUtils::TryParse<float>(data, speed)) return;
	SetCurrentSpeed(speed);
}

void MovementAIComponent::HandleWaypointCommandRemoveNPC(const std::string& data) {
	if (data.empty()) return;
	auto* proximityMonitorComponent = m_Parent->GetComponent<ProximityMonitorComponent>();
	if (!proximityMonitorComponent) {
		Game::logger->LogDebug("MovementAIComponent::HandleWaypointCommandRemoveNPC", "Proximity monitor component not found!");
		return;
	}
	const auto foundObjs = proximityMonitorComponent->GetProximityObjects("KillOBJS");
	for (auto& [objid, phyEntity] : foundObjs){
		auto entity = Game::entityManager->GetEntity(objid);
		if (!entity) return;
		auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
		if (!destroyableComponent) {
			Game::logger->LogDebug("MovementAIComponent::HandleWaypointCommandRemoveNPC", "Destroyable component not found!");
			return;
		}
		uint32_t factionID = -1;
		if (!GeneralUtils::TryParse<uint32_t>(data, factionID)) return;
		if (destroyableComponent->BelongsToFaction(factionID)) m_Parent->Kill();
	}
}

void MovementAIComponent::HandleWaypointCommandChangeWaypoint(const std::string& data) {
	std::string path_string = "";
	int32_t index = 0;
	// sometimes there's a path and what waypoint to start, which are comma separated
	if (data.find(",") != std::string::npos){
		auto datas = GeneralUtils::SplitString(data, ',');
		path_string = datas.at(0);
		if (!GeneralUtils::TryParse(datas.at(1), index)) return;
	} else path_string = data;

	if (path_string != "") {
		SetPathStartingWaypointIndex(index);
		SetupPath(path_string);
	}
}

void MovementAIComponent::HandleWaypointCommandSpawnObject(const std::string& data) {
	LOT newObjectLOT = 0;
	if (!GeneralUtils::TryParse(data, newObjectLOT)) return;
	EntityInfo info{};
	info.lot = newObjectLOT;
	info.pos = m_Parent->GetPosition();
	info.rot = m_Parent->GetRotation();
	auto* spawnedEntity = Game::entityManager->CreateEntity(info, nullptr, m_Parent);
	Game::entityManager->ConstructEntity(spawnedEntity);
	m_Parent->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
}
