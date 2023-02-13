#include "AgImagSmashable.h"
#include "EntityManager.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"

void AgImagSmashable::OnDie(Entity* self, Entity* killer) {
	bool maxImagGreaterThanZero = false;

	if (killer) {
		DestroyableComponent* dest = static_cast<DestroyableComponent*>(killer->GetComponent(COMPONENT_TYPE_DESTROYABLE));
		if (dest) {
			maxImagGreaterThanZero = dest->GetMaxImagination() > 0;
		}

		if (maxImagGreaterThanZero) {
			int amount = GeneralUtils::GenerateRandomNumber<int>(0, 3);
			for (int i = 0; i < amount; ++i) {
				GameMessages::SendDropClientLoot(killer, self->GetObjectID(), 935, 0, self->GetPosition());
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

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);
		if (newEntity) {
			EntityManager::Instance()->ConstructEntity(newEntity);
		}
	}
}
