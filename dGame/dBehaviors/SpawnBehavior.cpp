#include "SpawnBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"
#include "RebuildComponent.h"

void SpawnBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* origin = EntityManager::Instance()->GetEntity(context->originator);

	if (origin == nullptr) {
		Game::logger->Log("SpawnBehavior", "Failed to find self entity (%llu)!", context->originator);

		return;
	}

	if (branch.isProjectile) {
		auto* target = EntityManager::Instance()->GetEntity(branch.target);

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

	auto* entity = EntityManager::Instance()->CreateEntity(
		info,
		nullptr,
		EntityManager::Instance()->GetEntity(context->originator)
	);

	if (entity == nullptr) {
		Game::logger->Log("SpawnBehavior", "Failed to spawn entity (%i)!", this->m_lot);

		return;
	}

	entity->SetOwnerOverride(context->originator);

	// Unset the flag to reposition the player, this makes it harder to glitch out of the map
	auto* rebuildComponent = entity->GetComponent<RebuildComponent>();

	if (rebuildComponent != nullptr) {
		rebuildComponent->SetRepositionPlayer(false);
	}

	EntityManager::Instance()->ConstructEntity(entity);

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
	auto* entity = EntityManager::Instance()->GetEntity(second);

	if (entity == nullptr) {
		Game::logger->Log("SpawnBehavior", "Failed to find spawned entity (%llu)!", second);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(COMPONENT_TYPE_DESTROYABLE));

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
