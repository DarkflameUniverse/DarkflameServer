#include "NpcCowboyServer.h"
#include "MissionState.h"
#include "InventoryComponent.h"

void NpcCowboyServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID != 1880) {
		return;
	}

	auto* inventoryComponent = target->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	if (missionState == MissionState::MISSION_STATE_COMPLETE_ACTIVE ||
		missionState == MissionState::MISSION_STATE_ACTIVE ||
		missionState == MissionState::MISSION_STATE_AVAILABLE ||
		missionState == MissionState::MISSION_STATE_COMPLETE_AVAILABLE) {
		if (inventoryComponent->GetLotCount(14378) == 0) {
			inventoryComponent->AddItem(14378, 1, eLootSourceType::LOOT_SOURCE_NONE);
		}
	} else if (missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE || missionState == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE) {
		inventoryComponent->RemoveItem(14378, 1);
	}
}
