#include "NpcNjAssistantServer.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "Item.h"
#include "eMissionState.h"

void NpcNjAssistantServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID != mailMission) return;

	if (missionState == eMissionState::COMPLETE || missionState == eMissionState::READY_TO_COMPLETE) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"switch", 0, 0, LWOOBJID_EMPTY, "", target->GetSystemAddress());

		auto* inv = static_cast<InventoryComponent*>(target->GetComponent(COMPONENT_TYPE_INVENTORY));

		// If we are ready to complete our missions, we take the kit from you:
		if (inv && missionState == eMissionState::READY_TO_COMPLETE) {
			auto* id = inv->FindItemByLot(14397); //the kit's lot

			if (id != nullptr) {
				inv->RemoveItem(id->GetLot(), id->GetCount());
			}
		}
	} else if (missionState == eMissionState::AVAILABLE) {
		auto* missionComponent = static_cast<MissionComponent*>(target->GetComponent(COMPONENT_TYPE_MISSION));
		missionComponent->CompleteMission(mailAchievement, true);
	}
}
