#include "FvDragonInstanceServer.h"

#include "Entity.h"
#include "DestroyableComponent.h"

void FvDragonInstanceServer::OnPlayerLoaded(Entity* self, Entity* player) {
	auto* const destComp = player->GetComponent<DestroyableComponent>();
	if (destComp) {
		destComp->SetHealth(destComp->GetMaxHealth());
		destComp->SetArmor(destComp->GetMaxArmor());
		destComp->SetImagination(destComp->GetMaxImagination());
		Game::entityManager->SerializeEntity(player);
	}
}
