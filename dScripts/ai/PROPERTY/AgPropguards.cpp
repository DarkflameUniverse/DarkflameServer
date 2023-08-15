#include "AgPropguards.h"
#include "Character.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "eMissionState.h"

void AgPropguards::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* character = target->GetCharacter();
	if (character == nullptr)
		return;

	const auto flag = GetFlagForMission(missionID);
	if (flag == 0)
		return;

	if ((missionState == eMissionState::AVAILABLE || missionState == eMissionState::ACTIVE)
		&& !character->GetPlayerFlag(flag)) {
		// If the player just started the mission, play a cinematic highlighting the target
		GameMessages::SendPlayCinematic(target->GetObjectID(), u"MissionCam", target->GetSystemAddress());
	} else if (missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		// Makes the guard disappear once the mission has been completed
		const auto zoneControlID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
		GameMessages::SendNotifyClientObject(zoneControlID, u"GuardChat", 0, 0, self->GetObjectID(),
			"", UNASSIGNED_SYSTEM_ADDRESS);

		self->AddCallbackTimer(5.0f, [self]() {
			auto spawnerName = self->GetVar<std::string>(u"spawner_name");
			if (spawnerName.empty())
				spawnerName = "Guard";

			auto spawners = Game::zoneManager->GetSpawnersByName(spawnerName);
			for (auto* spawner : spawners) {
				spawner->Deactivate();
			}

			self->Smash();
			});
	}
}

int32_t AgPropguards::GetFlagForMission(uint32_t missionID) {
	switch (missionID) {
	case 872:
		return 97;
	case 873:
		return 98;
	case 874:
		return 99;
	case 1293:
		return 118;
	case 1322:
		return 122;
	default:
		return 0;
	}
}
