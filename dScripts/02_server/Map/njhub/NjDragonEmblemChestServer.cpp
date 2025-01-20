#include "NjDragonEmblemChestServer.h"
#include "Loot.h"
#include "Entity.h"
#include "DestroyableComponent.h"
#include "ePlayerFlag.h"

void NjDragonEmblemChestServer::OnUse(Entity* self, Entity* user) {
	GameMessages::SetFlag setFlag{};
	setFlag.target = user->GetObjectID();
	setFlag.iFlagId = ePlayerFlag::NJ_WU_SHOW_DAILY_CHEST;
	setFlag.bFlag = false;
	SEND_ENTITY_MSG(setFlag);

	auto* destroyable = self->GetComponent<DestroyableComponent>();
	if (destroyable != nullptr) {
		Loot::DropLoot(user, self, destroyable->GetLootMatrixID(), 0, 0);
	}
}
