#include "ForceVolumeServer.h"
#include "PhantomPhysicsComponent.h"
#include "EntityManager.h"

void ForceVolumeServer::OnStartup(Entity* self) {
	auto* phantomPhysicsComponent = self->GetComponent<PhantomPhysicsComponent>();

	if (phantomPhysicsComponent == nullptr) return;

	const auto forceAmount = self->GetVar<float>(u"ForceAmt");
	const auto forceX = self->GetVar<float>(u"ForceX");
	const auto forceY = self->GetVar<float>(u"ForceY");
	const auto forceZ = self->GetVar<float>(u"ForceZ");

	phantomPhysicsComponent->SetEffectType(0); // PUSH
	phantomPhysicsComponent->SetDirectionalMultiplier(forceAmount);
	phantomPhysicsComponent->SetDirection({ forceX, forceY, forceZ });
	phantomPhysicsComponent->SetPhysicsEffectActive(true);

	EntityManager::Instance()->SerializeEntity(self);
}
