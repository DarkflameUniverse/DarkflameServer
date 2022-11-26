#pragma once
#include "CppScripts.h"

class AgPropguards : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
private:
	static uint32_t GetFlagForMission(uint32_t missionID);
};
