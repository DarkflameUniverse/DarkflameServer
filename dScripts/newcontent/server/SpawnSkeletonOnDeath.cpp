#include "SpawnSkeletonOnDeath.h"
#include "GeneralUtils.h"
#include "EntityInfo.h"
#include "Entity.h"
#include "EntityManager.h"

void SpawnSkeletonOnDeath::OnDie(Entity* self, Entity* killer) {
	auto chance = GeneralUtils::GenerateRandomNumber<uint8_t>(65, 70);
	if (chance == 69){
		EntityInfo info{};
		info.lot = 20083;
		info.pos = self->GetPosition();
		info.spawnerID = self->GetParentEntity()->GetObjectID();
		Game::entityManager->CreateEntity(info, nullptr, self->GetParentEntity());
	}
}