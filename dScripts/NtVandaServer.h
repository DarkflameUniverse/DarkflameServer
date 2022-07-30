#pragma once
#include "CppScripts.h"

class NtVandaServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	const uint32_t m_AlienPartMissionID = 1183;
	const std::vector<LOT> m_AlienPartLots = { 12479, 12480, 12481 };
};
