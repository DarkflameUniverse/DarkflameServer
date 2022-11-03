#include "FvFreeGfNinjas.h"
#include "Character.h"
#include "MissionComponent.h"

void FvFreeGfNinjas::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID == 705 && missionState == MissionState::MISSION_STATE_AVAILABLE) {
		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent == nullptr)
			return;

		missionComponent->AcceptMission(701);
		missionComponent->AcceptMission(702);
		missionComponent->AcceptMission(703);
		missionComponent->AcceptMission(704);

		auto* character = target->GetCharacter();
		if (character != nullptr)
			character->SetPlayerFlag(68, true);
	} else if (missionID == 786) {
		auto* character = target->GetCharacter();
		if (character != nullptr)
			character->SetPlayerFlag(81, true);
	}
}

void FvFreeGfNinjas::OnUse(Entity* self, Entity* user) {
	// To allow player who already have the mission to progress.
	auto* missionComponent = user->GetComponent<MissionComponent>();
	if (missionComponent == nullptr)
		return;

	if (missionComponent->GetMissionState(705) == MissionState::MISSION_STATE_ACTIVE) {
		auto* character = user->GetCharacter();
		if (character != nullptr)
			character->SetPlayerFlag(68, true);

		missionComponent->AcceptMission(701, true);
		missionComponent->AcceptMission(702, true);
		missionComponent->AcceptMission(703, true);
		missionComponent->AcceptMission(704, true);
	}
}
