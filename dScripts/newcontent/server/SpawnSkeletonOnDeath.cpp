#include "SpawnSkeletonOnDeath.h"
#include "GeneralUtils.h"
#include "EntityInfo.h"
#include "Entity.h"
#include "EntityManager.h"

void SpawnSkeletonOnDeath::OnDie(Entity* self, Entity* killer) {
	auto chance = GeneralUtils::GenerateRandomNumber<uint8_t>(1, 30);
	if (chance <= 15){
		EntityInfo info{};
		info.lot = 20083;
		info.pos = self->GetPosition();
		info.spawnerID = killer->GetObjectID();
		auto skellyboi = Game::entityManager->CreateEntity(info, nullptr, killer);
		Game::entityManager->ConstructEntity(skellyboi);
	} else if (chance == 20) {
		EntityInfo info{};
		info.lot = 20085;
		info.pos = self->GetPosition();
		info.spawnerID = killer->GetObjectID();
		auto skellyboi = Game::entityManager->CreateEntity(info, nullptr, killer);
		Game::entityManager->ConstructEntity(skellyboi);
	}
}