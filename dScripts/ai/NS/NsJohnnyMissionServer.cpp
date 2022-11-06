#include "NsJohnnyMissionServer.h"
#include "MissionComponent.h"

void NsJohnnyMissionServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID == 773 && missionState <= MissionState::MISSION_STATE_ACTIVE) {
		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->AcceptMission(774);
			missionComponent->AcceptMission(775);
			missionComponent->AcceptMission(776);
			missionComponent->AcceptMission(777);
		}
	}
}
