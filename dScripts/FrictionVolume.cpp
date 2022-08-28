#include "FrictionVolume.h"
#include "PhantomPhysicsComponent.h"
#include "Entity.h"

void FrictionVolume::OnStartup(Entity* self) {
	auto frictionAmount = self->GetVarAs<float>(m_FrictionVariable);
	if (!frictionAmount) frictionAmount = m_DefaultFriction;
	auto* phanPhys = self->GetComponent<PhantomPhysicsComponent>();
	if (!phanPhys) return;

	phanPhys->SetPhysicsEffectActive(true);
	phanPhys->SetEffectType(4); // this should be noted in a enum somewhere
	phanPhys->SetDirectionalMultiplier(frictionAmount);
}
