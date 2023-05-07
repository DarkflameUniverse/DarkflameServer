#pragma once
#include "CppScripts.h"

class FvBounceOverWall : public CppScripts::Script
{
	/**
	 * @brief When a collision has been made with self this method is called.
	 *
	 * @param self The Entity that called this function.
	 * @param target The target Entity of self.
	 */
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	/**
	 * MissionId for the Gate Crasher mission.
	 */
	int32_t GateCrasherMissionId = 849;
	/**
	 * MissionUid for the Gate Crasher mission.
	 */
	int32_t GateCrasherMissionUid = 1241;
};
