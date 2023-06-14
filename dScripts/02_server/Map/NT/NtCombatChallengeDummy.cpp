#include "NtCombatChallengeDummy.h"
#include "EntityManager.h"

void NtCombatChallengeDummy::OnDie(Entity* self, Entity* killer) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		challengeObject->GetScript()->OnDie(challengeObject, killer);
	}
}

void NtCombatChallengeDummy::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		challengeObject->GetScript()->OnHitOrHealResult(challengeObject, attacker, damage);
	}
}
