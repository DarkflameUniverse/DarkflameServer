#include "RepairBehavior.h"
#include "BehaviorBranchContext.h"
#include "DestroyableComponent.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "Logger.h"
#include "Game.h"
#include "eReplicaComponentType.h"

void RepairBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
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

	destroyable->Repair(this->m_armor);
}

void RepairBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}

void RepairBehavior::Load() {
	this->m_armor = GetInt("armor");
}
