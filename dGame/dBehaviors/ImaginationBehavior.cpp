#include "ImaginationBehavior.h"
#include "BehaviorBranchContext.h"
#include "DestroyableComponent.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "Logger.h"


void ImaginationBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	auto* entity = Game::entityManager->GetEntity(branch.target);

	if (entity == nullptr) {
		return;
	}

	auto* destroyable = entity->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	destroyable->Imagine(this->m_imagination);

}

void ImaginationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, const BehaviorBranchContext branch) {
	Handle(context, bit_stream, branch);
}

void ImaginationBehavior::Load() {
	this->m_imagination = GetInt("imagination");
}
