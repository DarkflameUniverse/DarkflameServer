#include "FrictionVolume.h"
#include "PhantomPhysicsComponent.h"
#include "Entity.h"
#include "dLogger.h"

void FrictionVolume::OnStartup(Entity* self) {
	Game::logger->Log("FrictionVolume", "LOT %i", self->GetLOT());
	auto frictionAmount = self->GetVarAs<float>(m_FrictionVariable);
	Game::logger->Log("FrictionVolume", "Lvl friction %f", frictionAmount);
	if (!frictionAmount) frictionAmount = m_DefaultFriction;
	auto* phantomPhysicsComponent = self->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;

	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(4); // this should be noted in a enum somewhere
	phantomPhysicsComponent->SetDirectionalMultiplier(frictionAmount);
}
