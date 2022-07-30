#include "NtDukeServer.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"

void NtDukeServer::SetVariables(Entity* self) {
	self->SetVar<float_t>(m_SpyProximityVariable, 35.0f);

	self->SetVar<SpyData>(m_SpyDataVariable, {
		NT_FACTION_SPY_DUKE, 13548, 1319
		});

	self->SetVar<std::vector<SpyDialogue>>(m_SpyDialogueTableVariable, {
			{ "DUKE_NT_CONVO_1", 0 },
			{ "DUKE_NT_CONVO_2", 0 },
			{ "DUKE_NT_CONVO_3", 0 },
		});

	// If there's an alternating conversation, indices should be provided using the conversationID variables
	self->SetVar<std::vector<LWOOBJID>>(m_SpyCinematicObjectsVariable, { self->GetObjectID() });
}

void NtDukeServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {

	// Handles adding and removing the sword for the Crux Prime Sword mission
	auto* missionComponent = target->GetComponent<MissionComponent>();
	auto* inventoryComponent = target->GetComponent<InventoryComponent>();

	if (missionComponent != nullptr && inventoryComponent != nullptr) {
		auto state = missionComponent->GetMissionState(m_SwordMissionID);
		auto lotCount = inventoryComponent->GetLotCount(m_SwordLot);

		if ((state == MissionState::MISSION_STATE_AVAILABLE || state == MissionState::MISSION_STATE_ACTIVE) && lotCount < 1) {
			inventoryComponent->AddItem(m_SwordLot, 1, eLootSourceType::LOOT_SOURCE_NONE);
		} else if (state == MissionState::MISSION_STATE_READY_TO_COMPLETE) {
			inventoryComponent->RemoveItem(m_SwordLot, lotCount);
		}
	}
}
