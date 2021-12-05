#include "RockHydrantSmashable.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "Game.h"
#include "dLogger.h"

void RockHydrantSmashable::OnDie(Entity* self, Entity* killer) {
	SimplePhysicsComponent* physics = self->GetComponent<SimplePhysicsComponent>();
	NiPoint3 pos = physics->GetPosition();

	EntityInfo info;
	info.lot = 12293;
	info.pos = pos;
	info.spawner = nullptr;
	info.spawnerID = self->GetSpawnerID();
	info.spawnerNodeID = 0;

	Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);
	if (newEntity) {
		EntityManager::Instance()->ConstructEntity(newEntity);
	}
}