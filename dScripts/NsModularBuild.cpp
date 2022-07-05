#include "NsModularBuild.h"
#include "MissionComponent.h"

void NsModularBuild::OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) {
	if (bCompleted) {
		MissionComponent* mission = static_cast<MissionComponent*>(player->GetComponent(COMPONENT_TYPE_MISSION));

		if (mission->GetMissionState(m_MissionNum) == MissionState::MISSION_STATE_ACTIVE) {
			for (LOT mod : modules) {
				if (mod == 9516 || mod == 9517 || mod == 9518) {
					mission->ForceProgress(m_MissionNum, 1178, 1);
				}
			}
		}
	}
}
