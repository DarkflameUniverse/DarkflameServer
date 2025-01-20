#include "NjNPCMissionSpinjitzuServer.h"

#include "eMissionState.h"

void NjNPCMissionSpinjitzuServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	const auto& element = self->GetVar<std::u16string>(ElementVariable);
	if (missionID == ElementMissions.at(element) && missionState >= eMissionState::READY_TO_COMPLETE) {

		const auto targetID = target->GetObjectID();

		// Wait for an animation to complete and flag that the player has learned spinjitzu
		self->AddCallbackTimer(5.0f, [targetID, element]() {
			GameMessages::SetFlag setFlag{};
			setFlag.target = targetID;
			setFlag.iFlagId = ElementFlags.at(element);
			setFlag.bFlag = true;
			SEND_ENTITY_MSG(setFlag);
			});
	}
}
