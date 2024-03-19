#include "NpcWispServer.h"
#include "InventoryComponent.h"
#include "dZoneManager.h"
#include "Entity.h"
#include "GameMessages.h"
#include "eMissionState.h"
#include "Spawner.h"

void NpcWispServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID != 1849 && missionID != 1883)
		return;

	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory == nullptr)
		return;

	LOT maelstromVacuumLot = 14592;
	auto* maelstromVacuum = inventory->FindItemByLot(maelstromVacuumLot);

	// For the daily we add the maelstrom vacuum if the player doesn't have it yet
	if (missionID == 1883 && (missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE)
		&& maelstromVacuum == nullptr) {
		inventory->AddItem(maelstromVacuumLot, 1, eLootSourceType::NONE);
	} else if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		inventory->RemoveItem(maelstromVacuumLot, 1);
	}

	// Next up hide or show the samples based on the mission state
	int32_t visible = 1;
	if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
		visible = 0;
	}

	auto groups = missionID == 1849
		? std::vector<std::string> { "MaelstromSamples" }
	: std::vector<std::string>{ "MaelstromSamples", "MaelstromSamples2ndary1", "MaelstromSamples2ndary2" };

	for (const auto& group : groups) {
		auto spawners = Game::zoneManager->GetSpawnersByName(group);
		for (const auto* spawner : spawners) {
			for (const auto objId : spawner->GetSpawnedObjectIDs())
			GameMessages::SendNotifyClientObject(objId, u"SetVisibility", visible, 0,
				target->GetObjectID(), "", target->GetSystemAddress());
		}
	}
}
