#include "NjDragonEmblemChestServer.h"
#include "Character.h"
#include "DestroyableComponent.h"

void NjDragonEmblemChestServer::OnUse(Entity* self, Entity* user) {
	auto* character = user->GetCharacter();
	if (character != nullptr) {
		character->SetPlayerFlag(NJ_WU_SHOW_DAILY_CHEST, false);
	}

	auto* destroyable = self->GetComponent<DestroyableComponent>();
	if (destroyable != nullptr) {
		LootGenerator::Instance().DropLoot(user, self, destroyable->GetLootMatrixID(), 0, 0);
	}
}
