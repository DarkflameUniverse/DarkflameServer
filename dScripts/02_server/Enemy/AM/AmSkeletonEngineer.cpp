#include "AmSkeletonEngineer.h"
#include "DestroyableComponent.h"
#include "SkillComponent.h"

void AmSkeletonEngineer::OnHit(Entity* self, Entity* attacker) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (destroyableComponent == nullptr || skillComponent == nullptr) {
		return;
	}

	if (destroyableComponent->GetHealth() < 12 && !self->GetVar<bool>(u"injured")) {
		self->SetVar(u"injured", true);

		skillComponent->CalculateBehavior(953, 19864, self->GetObjectID(), true);

		const auto attackerID = attacker->GetObjectID();

		self->AddCallbackTimer(4.5f, [this, self, attackerID]() {
			self->Smash(attackerID);
			});
	}
}
