#pragma once
#include "CppScripts.h"

class GfJailkeepMission final : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};
