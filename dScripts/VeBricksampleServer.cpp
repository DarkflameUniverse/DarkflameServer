#include "VeBricksampleServer.h"
#include "InventoryComponent.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "GameMessages.h"

void VeBricksampleServer::OnUse(Entity* self, Entity* user) {
	auto* missionComponent = user->GetComponent<MissionComponent>();
	if (missionComponent != nullptr && missionComponent->GetMissionState(1183) == MissionState::MISSION_STATE_ACTIVE) {
		const auto loot = self->GetVar<int32_t>(m_LootVariable);
		auto* inventoryComponent = user->GetComponent<InventoryComponent>();

		if (loot && inventoryComponent != nullptr && inventoryComponent->GetLotCount(loot) == 0) {
			inventoryComponent->AddItem(loot, 1, eLootSourceType::LOOT_SOURCE_ACTIVITY);

			for (auto* brickEntity : EntityManager::Instance()->GetEntitiesInGroup("Bricks")) {
				GameMessages::SendNotifyClientObject(brickEntity->GetObjectID(), u"Pickedup");
			}
		}
	}
}
