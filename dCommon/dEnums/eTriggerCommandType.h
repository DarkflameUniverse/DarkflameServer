#ifndef __ETRIGGERCOMMANDTYPE__H__
#define __ETRIGGERCOMMANDTYPE__H__

// For info about Trigger Command see:
// https://docs.lu-dev.net/en/latest/file-structures/lutriggers.html?highlight=trigger#possible-values-commands

enum class eTriggerCommandType {
	INVALID,
	ZONE_PLAYER,
	FIRE_EVENT,
	DESTROY_OBJ,
	TOGGLE_TRIGGER,
	RESET_REBUILD,
	SET_PATH,
	SET_PICK_TYPE,
	MOVE_OBJECT,
	ROTATE_OBJECT,
	PUSH_OBJECT,
	REPEL_OBJECT,
	SET_TIMER,
	CANCEL_TIMER,
	PLAY_CINEMATIC,
	TOGGLE_BBB,
	UPDATE_MISSION,
	SET_BOUNCER_STATE,
	BOUNCE_ALL_ON_BOUNCER,
	TURN_AROUND_ON_PATH,
	GO_FORWARD_ON_PATH,
	GO_BACKWARD_ON_PATH,
	STOP_PATHING,
	START_PATHING,
	LOCK_OR_UNLOCK_CONTROLS,
	PLAY_EFFECT,
	STOP_EFFECT,
	ACTIVATE_MUSIC_CUE,
	DEACTIVATE_MUSIC_CUE,
	FLASH_MUSIC_CUE,
	SET_MUSIC_PARAMETER,
	PLAY_2D_AMBIENT_SOUND,
	STOP_2D_AMBIENT_SOUND,
	PLAY_3D_AMBIENT_SOUND,
	STOP_3D_AMBIENT_SOUND,
	ACTIVATE_MIXER_PROGRAM,
	DEACTIVATE_MIXER_PROGRAM,
	CAST_SKILL,
	DISPLAY_ZONE_SUMMARY,
	SET_PHYSICS_VOLUME_EFFECT,
	SET_PHYSICS_VOLUME_STATUS,
	SET_MODEL_TO_BUILD,
	SPAWN_MODEL_BRICKS,
	ACTIVATE_SPAWNER_NETWORK,
	DEACTIVATE_SPAWNER_NETWORK,
	RESET_SPAWNER_NETWORK,
	DESTROY_SPAWNER_NETWORK_OBJECTS,
	GO_TO_WAYPOINT,
	ACTIVATE_PHYSICS
};


class TriggerCommandType {
public:
	static eTriggerCommandType StringToTriggerCommandType(std::string commandString) {
		const std::map<std::string, eTriggerCommandType> TriggerCommandMap = {
			{ "zonePlayer", eTriggerCommandType::ZONE_PLAYER},
			{ "fireEvent", eTriggerCommandType::FIRE_EVENT},
			{ "destroyObj", eTriggerCommandType::DESTROY_OBJ},
			{ "toggleTrigger", eTriggerCommandType::TOGGLE_TRIGGER},
			{ "resetRebuild", eTriggerCommandType::RESET_REBUILD},
			{ "setPath", eTriggerCommandType::SET_PATH},
			{ "setPickType", eTriggerCommandType::SET_PICK_TYPE},
			{ "moveObject", eTriggerCommandType::MOVE_OBJECT},
			{ "rotateObject", eTriggerCommandType::ROTATE_OBJECT},
			{ "pushObject", eTriggerCommandType::PUSH_OBJECT},
			{ "repelObject", eTriggerCommandType::REPEL_OBJECT},
			{ "setTimer", eTriggerCommandType::SET_TIMER},
			{ "cancelTimer", eTriggerCommandType::CANCEL_TIMER},
			{ "playCinematic", eTriggerCommandType::PLAY_CINEMATIC},
			{ "toggleBBB", eTriggerCommandType::TOGGLE_BBB},
			{ "updateMission", eTriggerCommandType::UPDATE_MISSION},
			{ "setBouncerState", eTriggerCommandType::SET_BOUNCER_STATE},
			{ "bounceAllOnBouncer", eTriggerCommandType::BOUNCE_ALL_ON_BOUNCER},
			{ "turnAroundOnPath", eTriggerCommandType::TURN_AROUND_ON_PATH},
			{ "goForwardOnPath", eTriggerCommandType::GO_FORWARD_ON_PATH},
			{ "goBackwardOnPath", eTriggerCommandType::GO_BACKWARD_ON_PATH},
			{ "stopPathing", eTriggerCommandType::STOP_PATHING},
			{ "startPathing", eTriggerCommandType::START_PATHING},
			{ "LockOrUnlockControls", eTriggerCommandType::LOCK_OR_UNLOCK_CONTROLS},
			{ "PlayEffect", eTriggerCommandType::PLAY_EFFECT},
			{ "StopEffect", eTriggerCommandType::STOP_EFFECT},
			{ "activateMusicCue", eTriggerCommandType::ACTIVATE_MUSIC_CUE},
			{ "deactivateMusicCue", eTriggerCommandType::DEACTIVATE_MUSIC_CUE},
			{ "flashMusicCue", eTriggerCommandType::FLASH_MUSIC_CUE},
			{ "setMusicParameter", eTriggerCommandType::SET_MUSIC_PARAMETER},
			{ "play2DAmbientSound", eTriggerCommandType::PLAY_2D_AMBIENT_SOUND},
			{ "stop2DAmbientSound", eTriggerCommandType::STOP_2D_AMBIENT_SOUND},
			{ "play3DAmbientSound", eTriggerCommandType::PLAY_3D_AMBIENT_SOUND},
			{ "stop3DAmbientSound", eTriggerCommandType::STOP_3D_AMBIENT_SOUND},
			{ "activateMixerProgram", eTriggerCommandType::ACTIVATE_MIXER_PROGRAM},
			{ "deactivateMixerProgram", eTriggerCommandType::DEACTIVATE_MIXER_PROGRAM},
			{ "CastSkill", eTriggerCommandType::CAST_SKILL},
			{ "displayZoneSummary", eTriggerCommandType::DISPLAY_ZONE_SUMMARY},
			{ "SetPhysicsVolumeEffect", eTriggerCommandType::SET_PHYSICS_VOLUME_EFFECT},
			{ "SetPhysicsVolumeStatus", eTriggerCommandType::SET_PHYSICS_VOLUME_STATUS},
			{ "setModelToBuild", eTriggerCommandType::SET_MODEL_TO_BUILD},
			{ "spawnModelBricks", eTriggerCommandType::SPAWN_MODEL_BRICKS},
			{ "ActivateSpawnerNetwork", eTriggerCommandType::ACTIVATE_SPAWNER_NETWORK},
			{ "DeactivateSpawnerNetwork", eTriggerCommandType::DEACTIVATE_SPAWNER_NETWORK},
			{ "ResetSpawnerNetwork", eTriggerCommandType::RESET_SPAWNER_NETWORK},
			{ "DestroySpawnerNetworkObjects", eTriggerCommandType::DESTROY_SPAWNER_NETWORK_OBJECTS},
			{ "Go_To_Waypoint", eTriggerCommandType::GO_TO_WAYPOINT},
			{ "ActivatePhysics", eTriggerCommandType::ACTIVATE_PHYSICS}
		};

		auto intermed = TriggerCommandMap.find(commandString);
		return (intermed != TriggerCommandMap.end()) ? intermed->second : eTriggerCommandType::INVALID;
	};
};

#endif  //!__ETRIGGERCOMMANDTYPE__H__
