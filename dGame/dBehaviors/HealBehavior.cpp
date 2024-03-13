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
		Log::Warn("Failed to find entity for ({})!", branch.target);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));

	if (destroyable == nullptr) {
		Log::Warn("Failed to find destroyable component for ({})!", branch.target);

		return;
	}

	destroyable->Heal(this->m_health);
}


void HealBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}


void HealBehavior::Load() {
	this->m_health = GetInt("health");
}
