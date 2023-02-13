#pragma once
#include "CppScripts.h"

class PirateRep : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
private:
	const int m_PirateRepMissionID = 301;
};
