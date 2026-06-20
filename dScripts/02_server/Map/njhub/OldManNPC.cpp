#include "OldManNPC.h"

#include "eMissionState.h"
#include "Character.h"
#include "MissionComponent.h"

void ResetMissions(Entity& user) {
	for (int32_t i = 1; i < 7; i++) {
		int32_t flag = 2020 + i;
		auto* const character = user.GetCharacter();
		if (character) character->SetPlayerFlag(flag, false);
	}
}

void OldManNPC::OnUse(Entity* self, Entity* user) {
	const auto* const missionComponent = user->GetComponent<MissionComponent>();
	if (!missionComponent) return;

	const auto* const mission = missionComponent->GetMission(2039);
	if (!mission) {
		ResetMissions(*user); // shouldnt be needed for dlu but it is because the mission is null
		return;
	}

	const auto missionState = mission->GetMissionState();
	if (missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE) {
		ResetMissions(*user);
	}
}
