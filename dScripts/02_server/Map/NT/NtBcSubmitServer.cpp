#include "NtBcSubmitServer.h"

#include <cstdint>
#include <map>

#include "Entity.h"
#include "MissionComponent.h"

// https://explorer.lu/missions/
// Key is the main mission, value is the breadcrumb mission to reset upon Mission Dialogue Ok.
// To see the actual missions, just append the number to the end of the URL.
namespace {
	std::map<uint32_t, uint32_t> ResetMissionsTable = {
		{999,  1335},
		{1002, 1355},
		{1006, 1349},
		{1009, 1348},
		{1379, 1335},
		{1380, 1355},
		{1378, 1349},
		{1377, 1348},
	};
}

void NtBcSubmitServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* missionComponent = target->GetComponent<MissionComponent>();
	if (!missionComponent) return;

	auto it = ResetMissionsTable.find(missionID);
	if (it == ResetMissionsTable.end()) return;

	const auto missionToReset = it->second;
	missionComponent->ResetMission(missionToReset);
}
