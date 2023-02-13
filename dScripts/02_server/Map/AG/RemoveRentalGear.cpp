#include "RemoveRentalGear.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "eMissionState.h"
#include "Character.h"

/*
--------------------------------------------------------------
--Removes the rental gear from the player on mission turn in
--
--created mrb ... 5 / 25 / 11
--updated abeechler 6 / 27 / 11 ... Add session flag resetting for set equips
--ported Max 21/07/2020
--------------------------------------------------------------
--add missionID configData to the object in HF to remove this
--gear what the specified mission is completed
--------------------------------------------------------------
*/

void RemoveRentalGear::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID != defaultMission && missionID != 313) return;

	if (missionState == eMissionState::COMPLETE || missionState == eMissionState::READY_TO_COMPLETE) {
		auto inv = static_cast<InventoryComponent*>(target->GetComponent(COMPONENT_TYPE_INVENTORY));
		if (!inv) return;

		//remove the inventory items
		for (int item : gearSets) {
			auto* id = inv->FindItemByLot(item);
			if (id) {
				inv->UnEquipItem(id);
				inv->RemoveItem(id->GetLot(), id->GetCount());
			}
		}

		//reset the equipment flag
		auto character = target->GetCharacter();
		if (character) character->SetPlayerFlag(equipFlag, false);
	}
}
