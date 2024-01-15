#pragma once
#include "CppScripts.h"
#include "NtBcSubmitServer.h"

class NtVandaServer : public NtBcSubmitServer {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
	const uint32_t m_AlienPartMissionID = 1183;
	const std::vector<LOT> m_AlienPartLots = { 12479, 12480, 12481 };
};
