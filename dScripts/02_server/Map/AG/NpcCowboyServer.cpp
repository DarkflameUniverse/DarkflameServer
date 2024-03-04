#include "NpcCowboyServer.h"
#include "eMissionState.h"
#include "InventoryComponent.h"
#include "dZoneManager.h"

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

	// Next up hide or show the samples based on the mission state
	int32_t visible = 1;
	if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		visible = 0;
	}

	auto spawners = Game::zoneManager->GetSpawnersByName("PlungerGunTargets");
	for (auto* spawner : spawners) {
		for (const auto entity : spawner->GetSpawnedObjectIDs())
			GameMessages::SendNotifyClientObject(entity, u"SetVisibility", visible, 0,
				target->GetObjectID(), "", target->GetSystemAddress());
	}
}
