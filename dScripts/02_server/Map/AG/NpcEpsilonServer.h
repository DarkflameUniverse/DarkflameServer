#pragma once
#include "CppScripts.h"

class NpcEpsilonServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState);
};

