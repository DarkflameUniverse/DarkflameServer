#ifndef MOVEMENTAICOMPONENT_H
#include "MovementAIComponent.h"
#endif
#include "eWaypointCommandType.h"

void MovementAIComponent::HandleWaypointArrived() {
	if (m_Path->pathWaypoints[m_CurrentPathWaypointIndex].commands.empty()) return;
	for(auto [command, data] : m_Path->pathWaypoints[m_CurrentPathWaypointIndex].commands){
		switch(command){
			case eWaypointCommandType::STOP:
				// call stop
				break;
			case eWaypointCommandType::GROUP_EMOTE:
				// get group
				// make them all emote
				break;
			case eWaypointCommandType::SET_VARIABLE:
				// set network variable???
				break;
			case eWaypointCommandType::CAST_SKILL:
				// just call cast skill lol
				break;
			case eWaypointCommandType::EQUIP_INVENTORY:
				// equip item via ID (not lot???)
				break;
			case eWaypointCommandType::UNEQUIP_INVENTORY:
				// unequip item via ID (not lot??)
				break;
			case eWaypointCommandType::DELAY:
				// number
				break;
			case eWaypointCommandType::EMOTE:
				// emote name
				break;
			case eWaypointCommandType::TELEPORT:
				// x,y,z
				break;
			case eWaypointCommandType::PATH_SPEED:
				// set speed?
				break;
			case eWaypointCommandType::REMOVE_NPC:
				// get objects in proximity
				// KillOBJS ???
				break;
			case eWaypointCommandType::CHANGE_WAYPOINT:
				// std::string path_string = "";
				// sometimes there's a path and what waypoint to start, which are comma separated
				// if (intermed.find(",") != std::string::npos){
				// 	auto datas = GeneralUtils::SplitString(intermed, ',');
				// 	path_string = datas[0];
				// 	m_PathIndex = stoi(datas[1]) - 1;
				// } else {
				// 	path_string = intermed;
				// 	m_PathIndex = 0;
				// }

				// if (path_string != "") {
				// 	SetMovementPath(const_cast<Path*>(dZoneManager::Instance()->GetZone()->GetPath(path_string)));
				// } else m_MovementPath = nullptr;
				break;
			case eWaypointCommandType::KILL_SELF:
				// Kill Silent
				break;
			case eWaypointCommandType::DELETE_SELF:
			case eWaypointCommandType::REMOVE_SELF:
				// Delete Object
				break;
			case eWaypointCommandType::SPAWN_OBJECT:
				// just make a new object
				break;
			case eWaypointCommandType::PLAY_SOUND:
				// msgPlayNDAudioEmitter
				break;
			case eWaypointCommandType::BOUNCE:
				Game::logger->Log("MovementAIComponentAronwk", "Unusable Command %i", command);
				break;
			case eWaypointCommandType::INVALID:
				Game::logger->LogDebug("MovementAIComponentAronwk", "Got invalid waypoint command %i", command);
				break;
		}
	}
}
