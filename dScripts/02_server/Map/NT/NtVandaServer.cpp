#include "NtVandaServer.h"
#include "InventoryComponent.h"
#include "eMissionState.h"

void NtVandaServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	// Removes the alien parts after completing the mission
	if (missionID == m_AlienPartMissionID && missionState == eMissionState::READY_TO_COMPLETE) {
		auto* inventoryComponent = target->GetComponent<InventoryComponent>();
		for (const auto& alienPartLot : m_AlienPartLots) {
			inventoryComponent->RemoveItem(alienPartLot, 1);
		}
	}
}
