#include "NpcCowboyServer.h"
#include "eMissionState.h"
#include "InventoryComponent.h"

void NpcCowboyServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID != 1880) {
		return;
	}

	auto* inventoryComponent = target->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	if (missionState == eMissionState::COMPLETE_ACTIVE ||
		missionState == eMissionState::ACTIVE ||
		missionState == eMissionState::AVAILABLE ||
		missionState == eMissionState::COMPLETE_AVAILABLE) {
		if (inventoryComponent->GetLotCount(14378) == 0) {
			inventoryComponent->AddItem(14378, 1, eLootSourceType::NONE);
		}
	} else if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		inventoryComponent->RemoveItem(14378, 1);
	}
}
