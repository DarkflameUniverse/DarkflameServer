#pragma once
#include "CppScripts.h"

class FvFreeGfNinjas : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
	void OnUse(Entity* self, Entity* user) override;
};
