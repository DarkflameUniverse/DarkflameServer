#include "SsModularBuildServer.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "eReplicaComponentType.h"

void SsModularBuildServer::OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) {
	int missionNum = 1732;

	if (bCompleted) {
		MissionComponent* mission = static_cast<MissionComponent*>(player->GetComponent(eReplicaComponentType::MISSION));
		Mission* rocketMission = mission->GetMission(missionNum);

		if (rocketMission->GetMissionState() == eMissionState::ACTIVE) {
			mission->ForceProgress(missionNum, 2478, 1);
		}
	}
}
