#include "PirateRep.h"
#include "Character.h"
#include "eMissionState.h"
#include "Entity.h"

void PirateRep::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID == m_PirateRepMissionID && missionState >= eMissionState::READY_TO_COMPLETE) {
		auto* character = target->GetCharacter();
		if (character) {
			character->SetPlayerFlag(ePlayerFlags::GF_PIRATE_REP, true);
		}
	}
}
