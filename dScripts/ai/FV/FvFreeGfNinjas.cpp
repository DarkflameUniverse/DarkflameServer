#include "FvFreeGfNinjas.h"

#include "MissionComponent.h"
#include "eMissionState.h"
#include "Game.h"
#include "EntityManager.h"

void FvFreeGfNinjas::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID == 705 && missionState == eMissionState::AVAILABLE) {
		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent == nullptr)
			return;

		missionComponent->AcceptMission(701);
		missionComponent->AcceptMission(702);
		missionComponent->AcceptMission(703);
		missionComponent->AcceptMission(704);

		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		setFlag.iFlagId = 68;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);
	} else if (missionID == 786) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		setFlag.iFlagId = 81;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);
	}
}

void FvFreeGfNinjas::OnUse(Entity* self, Entity* user) {
	// To allow player who already have the mission to progress.
	auto* missionComponent = user->GetComponent<MissionComponent>();
	if (missionComponent == nullptr)
		return;

	if (missionComponent->GetMissionState(705) == eMissionState::ACTIVE) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = user->GetObjectID();
		setFlag.iFlagId = 68;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);

		missionComponent->AcceptMission(701, true);
		missionComponent->AcceptMission(702, true);
		missionComponent->AcceptMission(703, true);
		missionComponent->AcceptMission(704, true);
	}
}
