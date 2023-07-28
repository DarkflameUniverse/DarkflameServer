#pragma once
#include "CppScripts.h"

class NpcPirateServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};
