#include "FvPassThroughWall.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"

void FvPassThroughWall::OnCollisionPhantom(Entity* self, Entity* target) {
	auto missionComponent = target->GetComponent<MissionComponent>();
	if (missionComponent == nullptr) return;

	//Because at the moment we do not have an ItemComponent component, we check to make sure a Maelstrom-Infused hood is equipped.  There are only three in the game right now.
	auto inventoryComponent = target->GetComponent<InventoryComponent>();
	// If no inventory component is found then abort.
	if (inventoryComponent == nullptr) return;
	// If no Maelstrom hoods are equipped then abort.
	if (!inventoryComponent->IsEquipped(WhiteMaelstromHood) && !inventoryComponent->IsEquipped(BlackMaelstromHood) && !inventoryComponent->IsEquipped(RedMaelstromHood)) return;

	// Progress mission Friend of the Ninja since all prerequisites are met.
	missionComponent->ForceProgress(friendOfTheNinjaMissionId, friendOfTheNinjaMissionUid, 1);
}
