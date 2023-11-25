#pragma once
#include "NtFactionSpyServer.h"
#include "NtBcSubmitServer.h"

class NtDukeServer : public NtFactionSpyServer, public NtBcSubmitServer {
	void SetVariables(Entity* self) override;
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
	const uint32_t m_SwordMissionID = 1448;
	const LOT m_SwordLot = 13777;
};
