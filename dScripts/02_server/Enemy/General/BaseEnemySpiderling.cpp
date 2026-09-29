#include "BaseEnemySpiderling.h"

#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "eStateChangeType.h"

void BaseEnemySpiderling::OnStartup(Entity* self) {
	// SetStunImmunity: attack, interrupt, move, turn, use item, equip, interact
	auto* controllablePhysicsComponent = self->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetStunImmunity(eStateChangeType::PUSH, self->GetObjectID(),
			true, true, true, false, true, true, true); // attack, equip, interact, jump, move, turn, use item
	}

	// SetStatusImmunity: pull to point and knockback; the interrupt immunity of SetStunImmunity lives here in DLU
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, true, true, false, false, false, false, true);
	}
}
