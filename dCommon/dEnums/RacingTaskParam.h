#pragma once

#include <cstdint>

enum class RacingTaskParam : int32_t {
	RACING_TASK_PARAM_FINISH_WITH_PLACEMENT = 1,            //<! A task param for finishing with a specific placement.
	RACING_TASK_PARAM_LAP_TIME = 2,                         //<! A task param for finishing with a specific lap time.
	RACING_TASK_PARAM_TOTAL_TRACK_TIME = 3,                 //<! A task param for finishing with a specific track time.
	RACING_TASK_PARAM_COMPLETE_ANY_RACING_TASK = 4,         //<! A task param for completing a racing task.
	RACING_TASK_PARAM_COMPLETE_TRACK_TASKS = 5,             //<! A task param for completing a task for a specific track.
	RACING_TASK_PARAM_MODULAR_BUILDING = 6,                 //<! A task param for modular building with racing builds.
	RACING_TASK_PARAM_SAFE_DRIVER = 10,                     //<! A task param for completing a race without smashing.
	RACING_TASK_PARAM_SMASHABLES = 11,                      //<! A task param for smashing entities during a race.
	RACING_TASK_PARAM_COLLECT_IMAGINATION = 12,             //<! A task param for collecting imagination during a race.
	RACING_TASK_PARAM_COMPETED_IN_RACE = 13,                //<! A task param for competing in a race.
	RACING_TASK_PARAM_WIN_RACE_IN_WORLD = 14,               //<! A task param for winning a race in a specific world.
	RACING_TASK_PARAM_FIRST_PLACE_MULTIPLE_TRACKS = 15,     //<! A task param for finishing in first place on multiple tracks.
	RACING_TASK_PARAM_LAST_PLACE_FINISH = 16,               //<! A task param for finishing in last place.
	RACING_TASK_PARAM_SMASH_SPECIFIC_SMASHABLE = 17         //<! A task param for smashing dragon eggs during a race.
};
