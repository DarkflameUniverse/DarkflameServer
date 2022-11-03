#pragma once
#include "CppScripts.h"

class FvFong : public CppScripts::Script
{
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
};
