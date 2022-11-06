#include "NpcPirateServer.h"
#include "InventoryComponent.h"

void NpcPirateServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory != nullptr && missionID == 1881) {
		auto* luckyShovel = inventory->FindItemByLot(14591);

		// Add or remove the lucky shovel based on whether the mission was completed or started
		if ((missionState == MissionState::MISSION_STATE_AVAILABLE || missionState == MissionState::MISSION_STATE_COMPLETE_AVAILABLE)
			&& luckyShovel == nullptr) {
			inventory->AddItem(14591, 1, eLootSourceType::LOOT_SOURCE_NONE);
		} else if (missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE || missionState == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE) {
			inventory->RemoveItem(14591, 1);
		}
	}
}
