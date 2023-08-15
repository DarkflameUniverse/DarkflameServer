#include "GfJailkeepMission.h"
#include "MissionComponent.h"
#include "Character.h"
#include "eMissionState.h"

void GfJailkeepMission::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* missionComponent = target->GetComponent<MissionComponent>();
	if (missionComponent == nullptr)
		return;

	if (missionID == 385 && missionState == eMissionState::AVAILABLE) {
		missionComponent->AcceptMission(386, true);
		missionComponent->AcceptMission(387, true);
		missionComponent->AcceptMission(388, true);
		missionComponent->AcceptMission(390, true);
	} else if (missionID == 385 && missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		auto* character = target->GetCharacter();
		if (character != nullptr && character->GetPlayerFlag(68)) {
			missionComponent->AcceptMission(701);
			missionComponent->AcceptMission(702);
			missionComponent->AcceptMission(703);
			missionComponent->AcceptMission(704);
		}
	}
}

void GfJailkeepMission::OnUse(Entity* self, Entity* user) {
	auto* missionComponent = user->GetComponent<MissionComponent>();
	if (missionComponent == nullptr)
		return;

	if (missionComponent->GetMissionState(385) == eMissionState::ACTIVE) {
		missionComponent->AcceptMission(386, true);
		missionComponent->AcceptMission(387, true);
		missionComponent->AcceptMission(388, true);
		missionComponent->AcceptMission(390, true);
	}
}

