#pragma once
#include "CppScripts.h"

class NpcNpSpacemanBob : public CppScripts::Script
{
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState);
};

