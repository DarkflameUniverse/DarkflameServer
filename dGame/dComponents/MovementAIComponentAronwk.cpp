#ifndef MOVEMENTAICOMPONENT_H
#include "MovementAIComponent.h"
#endif
#include "eWaypointCommandType.h"

void MovementAIComponent::HandleWaypointArrived() {
	if (m_Path->pathWaypoints[m_CurrentPathWaypointIndex].commands.empty()) return;
	for(auto [command, data] : m_Path->pathWaypoints[m_CurrentPathWaypointIndex].commands){
		switch(command){
			case eWaypointCommandType::BOUNCE:
				break;
			case eWaypointCommandType::STOP:
				break;
			case eWaypointCommandType::GROUP_EMOTE:
				break;
			case eWaypointCommandType::SET_VARIABLE:
				break;
			case eWaypointCommandType::CAST_SKILL:
				break;
			case eWaypointCommandType::EQUIP_INVENTORY:
				break;
			case eWaypointCommandType::UNEQUIP_INVENTORY:
				break;
			case eWaypointCommandType::DELAY:
				break;
			case eWaypointCommandType::EMOTE:
				break;
			case eWaypointCommandType::TELEPORT:
				break;
			case eWaypointCommandType::PATH_SPEED:
				break;
			case eWaypointCommandType::REMOVE_NPC:
				break;
			case eWaypointCommandType::CHANGE_WAYPOINT:
				break;
			case eWaypointCommandType::DELETE_SELF:
				break;
			case eWaypointCommandType::KILL_SELF:
				break;
			case eWaypointCommandType::REMOVE_SELF:
				break;
			case eWaypointCommandType::SPAWN_OBJECT:
				break;
			case eWaypointCommandType::PLAY_SOUND:
				break;
			case eWaypointCommandType::INVALID:
				Game::logger->LogDebug("MovementAIComponentAronwk", "Got invalid waypoint command %i", command);
		}
	}
}
