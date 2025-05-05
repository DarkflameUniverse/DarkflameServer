#include "AmTeapotServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Item.h"
#include "eTerminateType.h"

void AmTeapotServer::OnUse(Entity* self, Entity* user) {
	auto* inventoryComponent = user->GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	// The client allows you to use the teapot only if you have a stack of 10 leaves in some inventory somewhere.
	if (inventoryComponent->GetLotCountNonTransfer(BLUE_FLOWER_LEAVES, false) >= 10) {
		inventoryComponent->RemoveItem(BLUE_FLOWER_LEAVES, 10, eInventoryType::ALL);
		inventoryComponent->AddItem(WU_S_IMAGINATION_TEA, 1);
	}

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}
