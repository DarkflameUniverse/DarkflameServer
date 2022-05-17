#include "AmTeapotServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"


void AmTeapotServer::OnUse(Entity* self, Entity* user) {
	auto* inventoryComponent = user->GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	if (inventoryComponent->GetLotCount(BLUE_FLOWER_LEAVES) >= 10){
		inventoryComponent->RemoveItem(BLUE_FLOWER_LEAVES, 10);
		inventoryComponent->AddItem(WU_S_IMAGINATION_TEA, 1);
	}
	GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}
