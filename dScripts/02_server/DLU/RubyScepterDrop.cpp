#include "RubyScepterDrop.h"

#include "Entity.h"
#include "InventoryComponent.h"
#include "GeneralUtils.h"

void RubyScepterDrop::OnDie(Entity* self, Entity* killer) {
	auto chance = GeneralUtils::GenerateRandomNumber<int32_t>(1, USHRT_MAX);
	if (chance == 1) {
		auto* inventoryComponent = killer->GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->AddItem(scepter, 1);
	}
}
