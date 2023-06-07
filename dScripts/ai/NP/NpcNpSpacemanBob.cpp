#include "NpcNpSpacemanBob.h"
#include "DestroyableComponent.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "eReplicaComponentType.h"

void NpcNpSpacemanBob::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionState == eMissionState::READY_TO_COMPLETE && missionID == 173) {
		auto destroyable = target->GetComponent<DestroyableComponent>();
		destroyable->SetImagination(6);
		auto mission = target->GetComponent<MissionComponent>();

		mission->CompleteMission(664);
	}
}
