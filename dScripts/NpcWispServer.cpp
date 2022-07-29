#include "NpcWispServer.h"
#include "InventoryComponent.h"
#include "EntityManager.h"
#include "Entity.h"
#include "GameMessages.h"

void NpcWispServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	if (missionID != 1849 && missionID != 1883)
		return;

	auto* inventory = target->GetComponent<InventoryComponent>();
	if (inventory == nullptr)
		return;

	LOT maelstromVacuumLot = 14592;
	auto* maelstromVacuum = inventory->FindItemByLot(maelstromVacuumLot);

	// For the daily we add the maelstrom vacuum if the player doesn't have it yet
	if (missionID == 1883 && (missionState == MissionState::MISSION_STATE_AVAILABLE || missionState == MissionState::MISSION_STATE_COMPLETE_AVAILABLE)
		&& maelstromVacuum == nullptr) {
		inventory->AddItem(maelstromVacuumLot, 1, eLootSourceType::LOOT_SOURCE_NONE);
	} else if (missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE || missionState == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE) {
		inventory->RemoveItem(maelstromVacuumLot, 1);
	}

	// Next up hide or show the samples based on the mission state
	auto visible = 1;
	if (missionState == MissionState::MISSION_STATE_READY_TO_COMPLETE || missionState == MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE) {
		visible = 0;
	}

	auto groups = missionID == 1849
		? std::vector<std::string> { "MaelstromSamples" }
	: std::vector<std::string>{ "MaelstromSamples", "MaelstromSamples2ndary1", "MaelstromSamples2ndary2" };

	for (const auto& group : groups) {
		auto samples = EntityManager::Instance()->GetEntitiesInGroup(group);
		for (auto* sample : samples) {
			GameMessages::SendNotifyClientObject(sample->GetObjectID(), u"SetVisibility", visible, 0,
				target->GetObjectID(), "", target->GetSystemAddress());
		}
	}
}
