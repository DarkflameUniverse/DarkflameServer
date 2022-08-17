#pragma once

#ifndef MISSIONTASKTYPE_H
#define MISSIONTASKTYPE_H

//! An enum for mission task types
enum class MissionTaskType : int {
	MISSION_TASK_TYPE_UNKNOWN = -1,                     //!< The task type is unknown
	MISSION_TASK_TYPE_SMASH = 0,                        //!< A task for smashing something
	MISSION_TASK_TYPE_SCRIPT = 1,                       //!< A task handled by a server LUA script
	MISSION_TASK_TYPE_ACTIVITY = 2,                     //!< A task for completing a quickbuild
	MISSION_TASK_TYPE_ENVIRONMENT = 3,                  //!< A task for something in the environment
	MISSION_TASK_TYPE_MISSION_INTERACTION = 4,          //!< A task for interacting with a mission
	MISSION_TASK_TYPE_EMOTE = 5,                        //!< A task for playing an emote
	MISSION_TASK_TYPE_FOOD = 9,                         //!< A task for eating food
	MISSION_TASK_TYPE_SKILL = 10,                       //!< A task for performing a skill
	MISSION_TASK_TYPE_ITEM_COLLECTION = 11,             //!< A task for collecting an item
	MISSION_TASK_TYPE_LOCATION = 12,                    //!< A task for finding a location
	MISSION_TASK_TYPE_MINIGAME = 14,                    //!< A task for doing something in a minigame
	MISSION_TASK_TYPE_NON_MISSION_INTERACTION = 15,     //!< A task for interacting with a non-mission
	MISSION_TASK_TYPE_MISSION_COMPLETE = 16,            //!< A task for completing a mission
	MISSION_TASK_TYPE_EARN_REPUTATION = 17,             //!< A task for earning reputation
	MISSION_TASK_TYPE_POWERUP = 21,                     //!< A task for collecting a powerup
	MISSION_TASK_TYPE_PET_TAMING = 22,                  //!< A task for taming a pet
	MISSION_TASK_TYPE_RACING = 23,                      //!< A task for racing
	MISSION_TASK_TYPE_PLAYER_FLAG = 24,                 //!< A task for setting a player flag
	MISSION_TASK_TYPE_PLACE_MODEL = 25,                //!< A task for picking up a model
	MISSION_TASK_TYPE_VISIT_PROPERTY = 30               //!< A task for visiting a property
};

#endif
