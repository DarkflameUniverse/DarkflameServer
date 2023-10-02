#include "SpawnBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "DestroyableComponent.h"
#include "RebuildComponent.h"
#include "Entity.h"
#include "EntityInfo.h"
#include "eReplicaComponentType.h"

void SpawnBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* origin = Game::entityManager->GetEntity(context->originator);

	if (origin == nullptr) {
		LOG("Failed to find self entity (%llu)!", context->originator);

		return;
	}

	if (branch.isProjectile) {
		auto* target = Game::entityManager->GetEntity(branch.target);

		if (target != nullptr) {
			origin = target;
		}
	}

	EntityInfo info;
	info.lot = this->m_lot;
	info.pos = origin->GetPosition();
	info.rot = origin->GetRotation();
	info.scale = 1;
	info.spawner = nullptr;
	info.spawnerID = context->originator;
	info.spawnerNodeID = 0;
	info.pos = info.pos + (info.rot.GetForwardVector() * m_Distance);

	auto* entity = Game::entityManager->CreateEntity(
		info,
		nullptr,
		Game::entityManager->GetEntity(context->originator)
	);

	if (entity == nullptr) {
		LOG("Failed to spawn entity (%i)!", this->m_lot);

		return;
	}

	entity->SetOwnerOverride(context->originator);

	// Unset the flag to reposition the player, this makes it harder to glitch out of the map
	auto* rebuildComponent = entity->GetComponent<RebuildComponent>();

	if (rebuildComponent != nullptr) {
		rebuildComponent->SetRepositionPlayer(false);
	}

	Game::entityManager->ConstructEntity(entity);

	if (branch.duration > 0) {
		context->RegisterTimerBehavior(this, branch, entity->GetObjectID());
	}

	if (branch.start != 0) {
		context->RegisterEndBehavior(this, branch, entity->GetObjectID());
	}

	entity->AddCallbackTimer(60, [entity]() {
		entity->Smash();
		});
}

void SpawnBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void SpawnBehavior::Timer(BehaviorContext* context, const BehaviorBranchContext branch, const LWOOBJID second) {
	auto* entity = Game::entityManager->GetEntity(second);

	if (entity == nullptr) {
		LOG("Failed to find spawned entity (%llu)!", second);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));

	if (destroyable == nullptr) {
		entity->Smash(context->originator);

		return;
	}

	destroyable->Smash(second);
}

void SpawnBehavior::End(BehaviorContext* context, const BehaviorBranchContext branch, const LWOOBJID second) {
	Timer(context, branch, second);
}


void SpawnBehavior::Load() {
	this->m_lot = GetInt("LOT_ID");
	this->m_Distance = GetFloat("distance");
}
