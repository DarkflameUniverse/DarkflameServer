#ifndef __EWAYPOINTCOMMANDTYPES__H__
#define __EWAYPOINTCOMMANDTYPES__H__

#include <cstdint>

enum class eWaypointCommandType : uint32_t {
	INVALID,
	BOUNCE,
	STOP,
	GROUP_EMOTE,
	SET_VARIABLE,
	CAST_SKILL,
	EQUIP_INVENTORY,
	UNEQUIP_INVENTORY,
	DELAY,
	EMOTE,
	TELEPORT,
	PATH_SPEED,
	REMOVE_NPC,
	CHANGE_WAYPOINT,
	DELETE_SELF,
	KILL_SELF,
	SPAWN_OBJECT,
	PLAY_SOUND,
};

class WaypointCommandType {
public:
	static eWaypointCommandType StringToWaypointCommandType(std::string commandString) {
		const std::map<std::string, eWaypointCommandType> WaypointCommandTypeMap = {
			{"bounce", eWaypointCommandType::BOUNCE},
			{"stop", eWaypointCommandType::STOP},
			{"groupemote", eWaypointCommandType::GROUP_EMOTE},
			{"setvar", eWaypointCommandType::SET_VARIABLE},
			{"castskill", eWaypointCommandType::CAST_SKILL},
			{"eqInvent", eWaypointCommandType::EQUIP_INVENTORY},
			{"unInvent", eWaypointCommandType::UNEQUIP_INVENTORY},
			{"delay", eWaypointCommandType::DELAY},
			{"femote", eWaypointCommandType::EMOTE},
			{"emote", eWaypointCommandType::EMOTE},
			{"teleport", eWaypointCommandType::TELEPORT},
			{"pathspeed", eWaypointCommandType::PATH_SPEED},
			{"removeNPC", eWaypointCommandType::REMOVE_NPC},
			{"changeWP", eWaypointCommandType::CHANGE_WAYPOINT},
			{"DeleteSelf", eWaypointCommandType::DELETE_SELF},
			{"killself", eWaypointCommandType::KILL_SELF},
			{"removeself", eWaypointCommandType::DELETE_SELF},
			{"spawnOBJ", eWaypointCommandType::SPAWN_OBJECT},
			{"playSound", eWaypointCommandType::PLAY_SOUND},
		};

		auto intermed = WaypointCommandTypeMap.find(commandString);
		return (intermed != WaypointCommandTypeMap.end()) ? intermed->second : eWaypointCommandType::INVALID;
	};
};


#endif  //!__EWAYPOINTCOMMANDTYPES__H__
