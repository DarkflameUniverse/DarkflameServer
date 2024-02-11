#include "NtNaomiBreadcrumbServer.h"
#include <map>

#include "eMissionState.h"
#include "MissionComponent.h"

// https://explorer.lu/missions/
// Key is the main mission, value is the breadcrumb mission to reset upon Mission Dialogue Ok.
// To see the actual missions, just append the number to the end of the URL.
namespace {
	std::map<int32_t, std::vector<int32_t>> CompleteBcMissionTable = {
		{1377, {1378, 1379, 1380, 1349, 1335, 1355}},
		{1378, {1377, 1379, 1380, 1348, 1335, 1355}},
		{1379, {1377, 1378, 1380, 1348, 1349, 1355}},
		{1380, {1377, 1378, 1379, 1348, 1349, 1335}},
	};

	std::map<int32_t, int32_t> MatchingBCTable = {
		{1377, 1348},
		{1378, 1349},
		{1379, 1335},
		{1380, 1355}
	};
}

void NtNaomiBreadcrumbServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* missionComponent = target->GetComponent<MissionComponent>();
	if (!missionComponent) return;

	auto itr = MatchingBCTable.find(missionID);
	if (itr == MatchingBCTable.end()) return;

	missionComponent->AcceptMission(itr->second);

	auto it = CompleteBcMissionTable.find(missionID);
	if (it == CompleteBcMissionTable.end()) return;

	if (missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE) {
		for (const auto& bcMission : it->second) {
			missionComponent->ResetMission(bcMission);
		}
	}
}
