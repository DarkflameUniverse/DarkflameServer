#pragma once
#include "CppScripts.h"

class DukeDialogueGlowingBrick : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
	void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) override;
};
