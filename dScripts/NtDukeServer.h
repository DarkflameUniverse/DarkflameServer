#pragma once
#include "NtFactionSpyServer.h"

class NtDukeServer : public NtFactionSpyServer {
	void SetVariables(Entity* self) override;
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	const uint32_t m_SwordMissionID = 1448;
	const LOT m_SwordLot = 13777;
};
