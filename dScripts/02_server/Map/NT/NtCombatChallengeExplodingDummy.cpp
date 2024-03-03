#include "NtCombatChallengeExplodingDummy.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"

void NtCombatChallengeExplodingDummy::OnDie(Entity* self, Entity* killer) {
	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = Game::entityManager->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		challengeObject->GetScript()->OnDie(challengeObject, killer);
	}
}

void NtCombatChallengeExplodingDummy::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	auto numTimesHit = self->GetVar<int32_t>(u"numTimesHit");
	if (destroyableComponent && numTimesHit == 0) {
		self->SetVar<int32_t>(u"numTimesHit", 1);
		destroyableComponent->SetHealth(destroyableComponent->GetHealth() / 2);
		return;
	} else if (numTimesHit == 2) {
		return;
	}
	self->SetVar<int32_t>(u"numTimesHit", 2);

	const auto challengeObjectID = self->GetVar<LWOOBJID>(u"challengeObjectID");

	auto* challengeObject = Game::entityManager->GetEntity(challengeObjectID);

	if (challengeObject != nullptr) {
		challengeObject->GetScript()->OnHitOrHealResult(challengeObject, attacker, damage);
	}
	auto skillComponent = self->GetComponent<SkillComponent>();
	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(1338, 30875, attacker->GetObjectID());
	}
	GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(self, u"camshake", self->GetObjectID(), 16.0f);
	self->Smash(attacker->GetObjectID());
}
