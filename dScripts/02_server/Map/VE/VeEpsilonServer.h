#pragma once
#include "CppScripts.h"

class VeEpsilonServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
	const uint32_t m_ConsoleMissionID = 1220;
	const uint32_t m_ConsoleRepeatMissionID = 1225;
	const uint32_t m_ConsoleBaseFlag = 1010;
	const std::string m_ConsoleGroup = "Consoles";
};
