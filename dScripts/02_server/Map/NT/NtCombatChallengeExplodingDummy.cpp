#include "NtCombatChallengeExplodingDummy.h"
#include "EntityManager.h"
#include "SkillComponent.h"

void NtCombatChallengeExplodingDummy::OnDie(Entity* self, Entity* killer) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(challengeObject)) {
			script->OnDie(challengeObject, killer);
		}
	}
}

void NtCombatChallengeExplodingDummy::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = EntityManager::Instance()->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(challengeObject)) {
			script->OnHitOrHealResult(challengeObject, attacker, damage);
		}
	}
	auto skillComponent = self->GetComponent<SkillComponent>();
	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(1338, 30875, attacker->GetObjectID());
	}
	self->Kill(attacker);
}
