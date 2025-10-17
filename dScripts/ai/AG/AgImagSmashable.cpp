#include "AgImagSmashable.h"
#include "EntityManager.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"
#include "Loot.h"

void AgImagSmashable::OnDie(Entity* self, Entity* killer) {
	bool maxImagGreaterThanZero = false;

	if (killer) {
		DestroyableComponent* dest = static_cast<DestroyableComponent*>(killer->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			maxImagGreaterThanZero = dest->GetMaxImagination() > 0;
		}

		if (maxImagGreaterThanZero) {
			int amount = GeneralUtils::GenerateRandomNumber<int>(0, 3);
			for (int i = 0; i < amount; ++i) {
				GameMessages::DropClientLoot lootMsg{};
				lootMsg.target = killer->GetObjectID();
				lootMsg.ownerID = killer->GetObjectID();
				lootMsg.sourceID = self->GetObjectID();
				lootMsg.item = 935;
				lootMsg.count = 1;
				lootMsg.spawnPos = self->GetPosition();
				Loot::DropItem(*killer, lootMsg);
			}
		}
	}

	CrateAnimal(self);
}

void AgImagSmashable::CrateAnimal(Entity* self) {
	int funnychance = GeneralUtils::GenerateRandomNumber<int>(0, 26);
	if (funnychance == 1) {
		EntityInfo info;
		info.lot = 8114;
		info.pos = self->GetPosition();
		info.spawner = nullptr;
		info.spawnerID = self->GetSpawnerID();
		info.spawnerNodeID = 0;

		Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);
		if (newEntity) {
			Game::entityManager->ConstructEntity(newEntity);
		}
	}
}
