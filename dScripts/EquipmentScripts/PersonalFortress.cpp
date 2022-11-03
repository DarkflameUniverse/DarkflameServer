#include "PersonalFortress.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"

void PersonalFortress::OnStartup(Entity* self) {
	auto* owner = self->GetOwner();
	self->AddTimer("FireSkill", 1.5);
	GameMessages::SendSetStunned(owner->GetObjectID(), PUSH, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true, true, true
	);

	auto* destroyableComponent = owner->GetComponent<DestroyableComponent>();

	if (destroyableComponent != nullptr) {
		destroyableComponent->PushImmunity();
	}

	EntityManager::Instance()->SerializeEntity(owner);
}

void PersonalFortress::OnDie(Entity* self, Entity* killer) {
	auto* owner = self->GetOwner();
	GameMessages::SendSetStunned(owner->GetObjectID(), POP, owner->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true, true, true
	);

	auto* destroyableComponent = owner->GetComponent<DestroyableComponent>();

	if (destroyableComponent != nullptr) {
		destroyableComponent->PopImmunity();
	}

	EntityManager::Instance()->SerializeEntity(owner);
}

void PersonalFortress::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FireSkill") {
		auto* owner = self->GetOwner();

		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(650, 13364, LWOOBJID_EMPTY, true, false);
	}
}
