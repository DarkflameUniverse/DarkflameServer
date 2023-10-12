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
		info.rot = self->GetRotation();
		info.spawnerID = killer->GetObjectID();
		auto skellyboi = Game::entityManager->CreateEntity(info, nullptr, killer);
		Game::entityManager->ConstructEntity(skellyboi);

	}
}