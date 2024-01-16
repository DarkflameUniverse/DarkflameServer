#include "FrictionVolumeServer.h"
#include "PhantomPhysicsComponent.h"
#include "ePhysicsEffectType.h"
#include "Game.h"
#include "EntityManager.h"

void FrictionVolumeServer::OnStartup(Entity* self) {
	auto frictionAmount = self->GetVar<float>(u"FrictionAmt");
	if (frictionAmount == 0.0f) frictionAmount = DefaultFrictionAmount;

	auto* phantomPhysicsComponent = self->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;

	phantomPhysicsComponent->SetEffectType(ePhysicsEffectType::FRICTION);
	phantomPhysicsComponent->SetDirectionalMultiplier(frictionAmount);
	phantomPhysicsComponent->SetPhysicsEffectActive(true);

	Game::entityManager->SerializeEntity(self);
}
