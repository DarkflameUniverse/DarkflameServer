#include "NjNPCMissionSpinjitzuServer.h"
#include "Character.h"
#include "EntityManager.h"

void NjNPCMissionSpinjitzuServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID,
	MissionState missionState) {

	const auto& element = self->GetVar<std::u16string>(ElementVariable);
	if (missionID == ElementMissions.at(element) && missionState >= MissionState::MISSION_STATE_READY_TO_COMPLETE) {

		const auto targetID = target->GetObjectID();

		// Wait for an animation to complete and flag that the player has learned spinjitzu
		self->AddCallbackTimer(5.0f, [targetID, element]() {
			auto* target = EntityManager::Instance()->GetEntity(targetID);
			if (target != nullptr) {
				auto* character = target->GetCharacter();
				if (character != nullptr) {
					character->SetPlayerFlag(ElementFlags.at(element), true);
				}
			}
			});
	}
}
