#include "FvPassThroughWall.h"
#include "InventoryComponent.h"

void FvPassThroughWall::OnCollisionPhantom(Entity* self, Entity* target) {
    auto missionComponent = target->GetComponent<MissionComponent>();
    if (missionComponent == nullptr) return;

    //Because at the moment we do not have an ItemComponent component, we check to make sure a Maelstrom-Infused hood is equipped.  There are only three in the game right now.
    auto inventoryComponent = target->GetComponent<InventoryComponent>();
    if (inventoryComponent == nullptr) return;
    if (!inventoryComponent->IsEquipped(WhiteMaelstromHood) && !inventoryComponent->IsEquipped(BlackMaelstromHood) && !inventoryComponent->IsEquipped(RedMaelstromHood)) return;

    missionComponent->ForceProgress(friendOfTheNinjaMissionId, friendOfTheNinjaMissionUid, 1);
}