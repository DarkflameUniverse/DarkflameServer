#include "AmTeapotServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Item.h"
#include "eTerminateType.h"

void AmTeapotServer::OnUse(Entity* self, Entity* user) {
	auto inventoryComponent = user->GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto* blueFlowerItem = inventoryComponent->FindItemByLot(BLUE_FLOWER_LEAVES, eInventoryType::ITEMS);
	if (!blueFlowerItem) {
		blueFlowerItem = inventoryComponent->FindItemByLot(BLUE_FLOWER_LEAVES, eInventoryType::VAULT_ITEMS);
		if (!blueFlowerItem) return;
	}

	// The client allows you to use the teapot only if you have a stack of 10 leaves in some inventory somewhere.
	if (blueFlowerItem->GetCount() >= 10) {
		blueFlowerItem->SetCount(blueFlowerItem->GetCount() - 10);
		inventoryComponent->AddItem(WU_S_IMAGINATION_TEA, 1);
	}
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
