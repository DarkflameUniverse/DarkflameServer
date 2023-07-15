#include "LootBuffBehavior.h"

void LootBuffBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto target = Game::entityManager->GetEntity(context->caster);
	if (!target) return;

	auto controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;

	controllablePhysicsComponent->AddPickupRadiusScale(m_Scale);
	Game::entityManager->SerializeEntity(target);

	if (branch.duration > 0) context->RegisterTimerBehavior(this, branch);

}

void LootBuffBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void LootBuffBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	auto target = Game::entityManager->GetEntity(context->caster);
	if (!target) return;

	auto controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;

	controllablePhysicsComponent->RemovePickupRadiusScale(m_Scale);
	Game::entityManager->SerializeEntity(target);
}

void LootBuffBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void LootBuffBehavior::Load() {
	this->m_Scale = GetFloat("scale");
}
