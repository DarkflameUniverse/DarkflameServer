#include "PirateRep.h"
#include "Character.h"

void PirateRep::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID == m_PirateRepMissionID && missionState >= MissionState::MISSION_STATE_READY_TO_COMPLETE) {
		auto* character = target->GetCharacter();
		if (character) {
			character->SetPlayerFlag(ePlayerFlags::GF_PIRATE_REP, true);
		}
	}
}
