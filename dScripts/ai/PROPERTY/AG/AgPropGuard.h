#pragma once
#include "CppScripts.h"

class AgPropGuard final : public CppScripts::Script
{
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;

private:
	std::vector<int> gearSets = { 14359,14321,14353,14315 };
};
