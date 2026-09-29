#pragma once
#include "CppScripts.h"

// WBL_Starflies (Portabello): the starflies start moving when the mission given here is handed in
class WblStarflies : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};
