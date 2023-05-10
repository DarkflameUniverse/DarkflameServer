#include "FvFong.h"
#include "Darkitect.h"
#include "eMissionState.h"

void FvFong::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID == 734 && missionState == eMissionState::READY_TO_COMPLETE) {
		Darkitect Baron;
		Baron.Reveal(self, target);
	}
}
