#include "ChangeOrientationBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "BaseCombatAIComponent.h"

void ChangeOrientationBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void ChangeOrientationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (!m_ToTarget) return; // TODO: Add the other arguments to this behavior

	auto* self = EntityManager::Instance()->GetEntity(context->originator);
	auto* other = EntityManager::Instance()->GetEntity(branch.target);

	if (self == nullptr || other == nullptr) return;

	const auto source = self->GetPosition();
	const auto destination = self->GetPosition();

	if (m_OrientCaster) {
		auto* baseCombatAIComponent = self->GetComponent<BaseCombatAIComponent>();

		/*if (baseCombatAIComponent != nullptr)
		{
			baseCombatAIComponent->LookAt(destination);
		}
		else*/
		{
			self->SetRotation(NiQuaternion::LookAt(source, destination));
		}

		EntityManager::Instance()->SerializeEntity(self);
	} else {
		other->SetRotation(NiQuaternion::LookAt(destination, source));

		EntityManager::Instance()->SerializeEntity(other);
	}
}

void ChangeOrientationBehavior::Load() {
	m_OrientCaster = GetBoolean("orient_caster");
	m_ToTarget = GetBoolean("to_target");
}
