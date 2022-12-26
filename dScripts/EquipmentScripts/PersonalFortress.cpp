#include "PersonalFortress.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"

void PersonalFortress::OnStartup(Entity* self) {
	auto* owner = self->GetOwner();
	self->AddTimer("FireSkill", 1.5);

	auto* destroyableComp = owner->GetComponent<DestroyableComponent>();
	if (destroyableComp) destroyableComp->SetStatusImmunity(
			eStateChangeType::PUSH,
			true, true, true, true, true, false, true, false, false
		);

	auto* controllablePhysicsComp = owner->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComp) controllablePhysicsComp->SetStunImmunity(
			eStateChangeType::PUSH, LWOOBJID_EMPTY,
			true, true, true, true, true, true
		);

	GameMessages::SendSetStunned(owner->GetObjectID(), eStateChangeType::PUSH, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true, true, true
	);

	EntityManager::Instance()->SerializeEntity(owner);
}

void PersonalFortress::OnDie(Entity* self, Entity* killer) {
	auto* owner = self->GetOwner();
	auto* destroyableComp = owner->GetComponent<DestroyableComponent>();
	if (destroyableComp) destroyableComp->SetStatusImmunity(
			eStateChangeType::POP,
			true, true, true, true, true, false, true, false, false
		);

	auto* controllablePhysicsComp = owner->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComp) controllablePhysicsComp->SetStunImmunity(
			eStateChangeType::POP, LWOOBJID_EMPTY,
			true, true, true, true, true, true
		);

	GameMessages::SendSetStunned(owner->GetObjectID(), eStateChangeType::POP, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true, true, true
	);

	EntityManager::Instance()->SerializeEntity(owner);
}

void PersonalFortress::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FireSkill") {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (skillComponent) skillComponent->CalculateBehavior(650, 13364, LWOOBJID_EMPTY, true, false);
	}
}
