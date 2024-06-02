#include "HealBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "Logger.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"


void HealBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);

	if (entity == nullptr) {
		LOG("Failed to find entity for (%llu)!", branch.target);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));

	if (destroyable == nullptr) {
		LOG("Failed to find destroyable component for %(llu)!", branch.target);

		return;
	}

	auto maxHealth = destroyable->GetMaxHealth();

	// 1 health is 5% of the max health, minimum of 5 health
	auto health = static_cast<int32_t>(maxHealth * 0.05f) * this->m_health;
	health = std::max(5u, health);

	destroyable->Heal(health);
}


void HealBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}


void HealBehavior::Load() {
	this->m_health = GetInt("health");
}
