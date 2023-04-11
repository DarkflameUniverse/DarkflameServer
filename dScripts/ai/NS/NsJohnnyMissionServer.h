#pragma once
#include "CppScripts.h"

class NsJohnnyMissionServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};
