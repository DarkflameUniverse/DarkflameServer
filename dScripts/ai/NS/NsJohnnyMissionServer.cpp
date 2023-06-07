#include "NsJohnnyMissionServer.h"
#include "MissionComponent.h"
#include "eMissionState.h"

void NsJohnnyMissionServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID == 773 && missionState <= eMissionState::ACTIVE) {
		auto missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->AcceptMission(774);
			missionComponent->AcceptMission(775);
			missionComponent->AcceptMission(776);
			missionComponent->AcceptMission(777);
		}
	}
}
