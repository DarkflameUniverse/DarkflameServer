#include "FvFong.h"
#include "Darkitect.h"
#include "MissionState.h"

void FvFong::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID == 734 && missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE) {
		Darkitect Baron;
		Baron.Reveal(self, target);
	}
}
