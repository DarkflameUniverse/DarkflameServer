#include "HealBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"


void HealBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	auto* entity = EntityManager::Instance()->GetEntity(branch.target);

	if (entity == nullptr) {
		Game::logger->Log("HealBehavior", "Failed to find entity for (%llu)!", branch.target);

		return;
	}

	auto destroyable = entity->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		Game::logger->Log("HealBehavior", "Failed to find destroyable component for %(llu)!", branch.target);

		return;
	}

	destroyable->Heal(this->m_health);
}


void HealBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}


void HealBehavior::Load() {
	this->m_health = GetInt("health");
}
