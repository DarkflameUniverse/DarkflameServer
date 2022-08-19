#include "RepairBehavior.h"
#include "BehaviorBranchContext.h"
#include "DestroyableComponent.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "Game.h"

void RepairBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	auto* entity = EntityManager::Instance()->GetEntity(branch.target);

	if (entity == nullptr) {
		Game::logger->Log("RepairBehavior", "Failed to find entity for (%llu)!", branch.target);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(COMPONENT_TYPE_DESTROYABLE));

	if (destroyable == nullptr) {
		Game::logger->Log("RepairBehavior", "Failed to find destroyable component for %(llu)!", branch.target);

		return;
	}

	destroyable->Repair(this->m_armor);
}

void RepairBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}

void RepairBehavior::Load() {
	this->m_armor = GetInt("armor");
}
