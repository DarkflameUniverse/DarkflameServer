#include "NpcNpSpacemanBob.h"
#include "DestroyableComponent.h"
#include "MissionComponent.h"
#include "eMissionState.h"

void NpcNpSpacemanBob::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionState == eMissionState::READY_TO_COMPLETE && missionID == 173) {
		DestroyableComponent* destroyable = static_cast<DestroyableComponent*>(target->GetComponent(COMPONENT_TYPE_DESTROYABLE));
		destroyable->SetImagination(6);
		MissionComponent* mission = static_cast<MissionComponent*>(target->GetComponent(COMPONENT_TYPE_MISSION));

		mission->CompleteMission(664);
	}
}
