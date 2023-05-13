#include "NpcPirateServer.h"
#include "eMissionState.h"
#include "InventoryComponent.h"

void NpcPirateServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory != nullptr && missionID == 1881) {
		auto* luckyShovel = inventory->FindItemByLot(14591);

		// Add or remove the lucky shovel based on whether the mission was completed or started
		if ((missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE)
			&& luckyShovel == nullptr) {
			inventory->AddItem(14591, 1, eLootSourceType::NONE);
		} else if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
			inventory->RemoveItem(14591, 1);
		}
	}
}
