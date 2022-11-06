#include "NtCombatChallengeDummy.h"
#include "EntityManager.h"

void NtCombatChallengeDummy::OnDie(Entity* self, Entity* killer) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(challengeObject)) {
			script->OnDie(challengeObject, killer);
		}
	}
}

void NtCombatChallengeDummy::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(challengeObject)) {
			script->OnHitOrHealResult(challengeObject, attacker, damage);
		}
	}
}
