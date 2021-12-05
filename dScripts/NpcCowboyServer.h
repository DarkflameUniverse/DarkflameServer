#pragma once
#include "CppScripts.h"

class NpcCowboyServer : public CppScripts::Script
{
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
};
