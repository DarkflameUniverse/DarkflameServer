#include "PirateRep.h"

#include "eMissionState.h"
#include "Entity.h"
#include "ePlayerFlag.h"

void PirateRep::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID == m_PirateRepMissionID && missionState >= eMissionState::READY_TO_COMPLETE) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		setFlag.iFlagId = ePlayerFlag::GF_PIRATE_REP;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);
	}
}
