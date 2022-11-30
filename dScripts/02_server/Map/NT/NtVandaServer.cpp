#include "NtVandaServer.h"
#include "InventoryComponent.h"

void NtVandaServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {

	// Removes the alien parts after completing the mission
	if (missionID == m_AlienPartMissionID && missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE) {
		auto* inventoryComponent = target->GetComponent<InventoryComponent>();
		for (const auto& alienPartLot : m_AlienPartLots) {
			inventoryComponent->RemoveItem(alienPartLot, 1);
		}
	}
}
