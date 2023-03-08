#include "StunImmunity.h"
#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"

void StunImmunity::OnStartup(Entity* self) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetStatusImmunity(
			eStateChangeType::PUSH, false, false, true, true, false, false, false, false, true
		);
	}

	auto* controllablePhysicsComponent = self->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::PUSH, self->GetObjectID(), true
		);
	}
}
