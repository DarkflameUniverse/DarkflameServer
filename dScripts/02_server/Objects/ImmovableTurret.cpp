#include "ImmovableTurret.h"

#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "eStateChangeType.h"

void ImmovableTurret::OnStartup(Entity* self) {
	// SetStunImmunity (attack, interrupt) and SetStatusImmunity (pull to point, knockback); DLU keeps interrupt with the status ones
	if (auto* controllablePhysics = self->GetComponent<ControllablePhysicsComponent>()) {
		controllablePhysics->SetStunImmunity(eStateChangeType::PUSH, self->GetObjectID(), true, false, false, false, false, false, false);
	}
	if (auto* destroyable = self->GetComponent<DestroyableComponent>()) {
		destroyable->SetStatusImmunity(eStateChangeType::PUSH, false, false, true, true, false, false, false, false, true);
	}
}
