#include "NjColeNPC.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"

void NjColeNPC::OnEmoteReceived(Entity* self, int32_t emote, Entity* target) {
	if (emote != 393) {
		return;
	}

	auto* inventoryComponent = target->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	if (!inventoryComponent->IsEquipped(14499) && !inventoryComponent->IsEquipped(16644)) {
		return;
	}

	auto* missionComponent = target->GetComponent<MissionComponent>();

	if (missionComponent == nullptr) {
		return;
	}

	missionComponent->ForceProgressTaskType(1818, 1, 1);
}

void NjColeNPC::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	NjNPCMissionSpinjitzuServer::OnMissionDialogueOK(self, target, missionID, missionState);

	if (missionID == 1818 && missionState >= MissionState::MISSION_STATE_READY_TO_COMPLETE) {
		auto* missionComponent = target->GetComponent<MissionComponent>();
		auto* inventoryComponent = target->GetComponent<InventoryComponent>();

		if (missionComponent == nullptr || inventoryComponent == nullptr) {
			return;
		}

		if (inventoryComponent->GetLotCount(14499) > 0) {
			inventoryComponent->RemoveItem(14499, 1);
		} else {
			return;
		}

		inventoryComponent->AddItem(16644, 1, eLootSourceType::LOOT_SOURCE_NONE);
	}
}
