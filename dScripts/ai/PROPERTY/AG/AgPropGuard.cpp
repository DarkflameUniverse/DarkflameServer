#include "AgPropGuard.h"
#include "Entity.h"
#include "Character.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "Item.h"
#include "eMissionState.h"

void AgPropGuard::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	auto* character = target->GetCharacter();
	auto missionComponent = target->GetComponent<MissionComponent>();
	auto inventoryComponent = target->GetComponent<InventoryComponent>();

	const auto state = missionComponent->GetMissionState(320);
	if (missionID == 768 && missionState == eMissionState::AVAILABLE) {
		if (!character->GetPlayerFlag(71)) {
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
	} else if (
		(missionID == 320 && state == eMissionState::AVAILABLE) /*||
		(state == eMissionState::COMPLETE && missionID == 891 && missionState == eMissionState::READY_TO_COMPLETE)*/
		) {
		//GameMessages::SendNotifyClientObject(EntityManager::Instance()->GetZoneControlEntity()->GetObjectID(), u"GuardChat", target->GetObjectID(), 0, target->GetObjectID(), "", target->GetSystemAddress());

		target->GetCharacter()->SetPlayerFlag(113, true);

		EntityManager::Instance()->GetZoneControlEntity()->AddTimer("GuardFlyAway", 1.0f);
	}
}
