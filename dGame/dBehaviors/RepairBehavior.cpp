#include "RepairBehavior.h"
#include "BehaviorBranchContext.h"
#include "DestroyableComponent.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "Game.h"
#include "eReplicaComponentType.h"
#include "LevelProgressionComponent.h"

void RepairBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);

	if (entity == nullptr) {
		Game::logger->Log("RepairBehavior", "Failed to find entity for (%llu)!", branch.target);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));

	if (destroyable == nullptr) {
		Game::logger->Log("RepairBehavior", "Failed to find destroyable component for %(llu)!", branch.target);

		return;
	}

	int32_t toApply = this->m_armor * 5;

	auto* levelProgressComponent = entity->GetComponent<LevelProgressionComponent>();

	if (levelProgressComponent != nullptr) {
		toApply *= levelProgressComponent->GetLevel();
	}

	// Apply a standard deviations of 20%
	toApply = static_cast<uint32_t>(toApply * (1.0f + (static_cast<float>(rand() % 40) / 100.0f) - 0.2f));

	destroyable->Repair(toApply);
}

void RepairBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}

void RepairBehavior::Load() {
	this->m_armor = GetInt("armor");
}
