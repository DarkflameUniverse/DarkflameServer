#pragma once
#include "CppScripts.h"

class NpcWispServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState);
};
