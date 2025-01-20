#include "AgPropGuard.h"

#include "Entity.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "Item.h"
#include "eMissionState.h"

void AgPropGuard::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* missionComponent = target->GetComponent<MissionComponent>();
	auto* inventoryComponent = target->GetComponent<InventoryComponent>();
	if (!missionComponent || !inventoryComponent) return;

	const auto state = missionComponent->GetMissionState(320);
	if (missionID == 768 && missionState == eMissionState::AVAILABLE) {
		GameMessages::GetFlag getFlag{};
		getFlag.target = target->GetObjectID();
		getFlag.iFlagId = 71;
		if (SEND_ENTITY_MSG(getFlag) && !getFlag.bFlag) {
			// TODO: Cinematic "MissionCam"
		}
	} else if (missionID == 768 && missionState >= eMissionState::READY_TO_COMPLETE) {
		//remove the inventory items
		for (int item : gearSets) {
			auto* id = inventoryComponent->FindItemByLot(item);

			if (id) {
				inventoryComponent->UnEquipItem(id);
				inventoryComponent->RemoveItem(id->GetLot(), id->GetCount());
			}
		}
	} else if (missionID == 320 && state == eMissionState::AVAILABLE) {
		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		setFlag.iFlagId = 113;
		setFlag.bFlag = true;
		SEND_ENTITY_MSG(setFlag);

		Game::entityManager->GetZoneControlEntity()->AddTimer("GuardFlyAway", 1.0f);
	}
}
