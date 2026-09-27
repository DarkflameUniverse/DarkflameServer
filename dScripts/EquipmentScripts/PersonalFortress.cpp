#include "PersonalFortress.h"
#include "GameMessages.h"
#include "CombatMessages.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "eStateChangeType.h"

void PersonalFortress::OnStartup(Entity* self) {
	auto* owner = self->GetOwner();
	self->AddTimer("FireSkill", 1.5);

	auto* destroyableComponent = owner->GetComponent<DestroyableComponent>();
	if (destroyableComponent) destroyableComponent->SetStatusImmunity(
			eStateChangeType::PUSH,
			true, true, true, true, true, false, true, false, false
		);

	auto* controllablePhysicsComponent = owner->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::PUSH, LWOOBJID_EMPTY,
			true, true, true, true, true, true
		);

	GameMessages::SetStunned stun;
	stun.target = owner->GetObjectID();
	stun.StateChangeType = eStateChangeType::PUSH;
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.bDontTerminateInteract = true;
	stun.Send(owner->GetSystemAddress());

	Game::entityManager->SerializeEntity(owner);
}

void PersonalFortress::OnDie(Entity* self, Entity* killer) {
	auto* owner = self->GetOwner();
	auto* destroyableComponent = owner->GetComponent<DestroyableComponent>();
	if (destroyableComponent) destroyableComponent->SetStatusImmunity(
			eStateChangeType::POP,
			true, true, true, true, true, false, true, false, false
		);

	auto* controllablePhysicsComponent = owner->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::POP, LWOOBJID_EMPTY,
			true, true, true, true, true, true
		);

	GameMessages::SetStunned stun;
	stun.target = owner->GetObjectID();
	stun.StateChangeType = eStateChangeType::POP;
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.bDontTerminateInteract = true;
	stun.Send(owner->GetSystemAddress());

	Game::entityManager->SerializeEntity(owner);
}

void PersonalFortress::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FireSkill") {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (skillComponent) skillComponent->CalculateBehavior(650, 13364, LWOOBJID_EMPTY, true, false);
	}
}
