#include "SsModularBuildServer.h"
#include "MissionComponent.h"
#include "eMissionState.h"

void SsModularBuildServer::OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) {
	int missionNum = 1732;

	if (bCompleted) {
		auto* const mission = player->GetComponent<MissionComponent>();
		// The player may never have had this mission, so check the state instead of the mission itself.
		if (mission && mission->GetMissionState(missionNum) == eMissionState::ACTIVE) {
			mission->ForceProgress(missionNum, 2478, 1);
		}
	}
}
