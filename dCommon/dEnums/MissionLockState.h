#pragma once

#ifndef MISSIONLOCKSTATE_H
#define MISSIONLOCKSTATE_H

enum class MissionLockState : int
{
	MISSION_LOCK_LOCKED,
	MISSION_LOCK_NEW,
	MISSION_LOCK_UNLOCKED,
};

#endif
