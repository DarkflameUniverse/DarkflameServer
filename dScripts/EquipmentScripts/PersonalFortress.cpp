#include "PersonalFortress.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "eStateChangeType.h"

void PersonalFortress::OnStartup(Entity* self) {
	auto* owner = self->GetOwner();
	if (owner == nullptr || owner == self) owner = self->GetParent();
	if (!owner) return;

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

	GameMessages::SendSetStunned(owner->GetObjectID(), eStateChangeType::PUSH, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, false, true, true, false, false, true
	);

	Game::entityManager->SerializeEntity(owner);

	const auto ownerId = owner->GetObjectID();
	self->AddDieCallback([ownerId]() {
		auto* pOwner = Game::entityManager->GetEntity(ownerId);
		if (!pOwner) return;

		auto* destComp = pOwner->GetComponent<DestroyableComponent>();
		if (destComp) destComp->SetStatusImmunity(
				eStateChangeType::POP,
				true, true, true, true, true, false, true, false, false
			);

		auto* physComp = pOwner->GetComponent<ControllablePhysicsComponent>();
		if (physComp) physComp->SetStunImmunity(
				eStateChangeType::POP, LWOOBJID_EMPTY,
				true, true, true, true, true, true
			);

		GameMessages::SendSetStunned(pOwner->GetObjectID(), eStateChangeType::POP, pOwner->GetSystemAddress(), LWOOBJID_EMPTY,
			true, true, true, false, true, true, false, false, true
		);

		Game::entityManager->SerializeEntity(pOwner);
	});
}

void PersonalFortress::OnDie(Entity* self, Entity* killer) {
	auto* owner = self->GetOwner();
	if (owner == nullptr || owner == self) owner = self->GetParent();
	if (!owner) return;

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

	GameMessages::SendSetStunned(owner->GetObjectID(), eStateChangeType::POP, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, false, true, true, false, false, true
	);

	Game::entityManager->SerializeEntity(owner);
}

void PersonalFortress::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FireSkill") {
		auto* skillComponent = self->GetComponent<SkillComponent>();
		auto* owner = self->GetOwner();
		const auto target = (owner != nullptr && owner != self) ? owner->GetObjectID() : (self->GetParent() ? self->GetParent()->GetObjectID() : self->GetObjectID());
		if (skillComponent) skillComponent->CastSkill(650, LWOOBJID_EMPTY, target);
	}
}
