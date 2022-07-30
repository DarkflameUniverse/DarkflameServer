#include "NjScrollChestServer.h"
#include "InventoryComponent.h"

void NjScrollChestServer::OnUse(Entity* self, Entity* user) {
	const auto keyLOT = self->GetVar<LOT>(u"KeyNum");
	const auto rewardItemLOT = self->GetVar<LOT>(u"openItemID");

	auto* playerInventory = user->GetComponent<InventoryComponent>();
	if (playerInventory != nullptr && playerInventory->GetLotCount(keyLOT) == 1) {

		// Check for the key and remove
		playerInventory->RemoveItem(keyLOT, 1);

		// Reward the player with the item set
		playerInventory->AddItem(rewardItemLOT, 1, eLootSourceType::LOOT_SOURCE_NONE);
	}
}
