#include "WaveBossApe.h"
#include "BaseCombatAIComponent.h"
#include "Entity.h"

void WaveBossApe::OnStartup(Entity* self) {
	BaseWavesGenericEnemy::OnStartup(self);

	self->SetVar<LOT>(u"QuickbuildAnchorLOT", 12900);
	self->SetVar<uint32_t>(u"GroundPoundSkill", 725);
	self->SetVar<float_t>(u"reviveTime", 12);
	self->SetVar<float_t>(u"AnchorDamageDelayTime", 0.5f);
	self->SetVar<float_t>(u"spawnQBTime", 5.0f);

	auto* combatAIComponent = self->GetComponent<BaseCombatAIComponent>();
	if (combatAIComponent != nullptr) {
		combatAIComponent->SetDisabled(true);
		combatAIComponent->SetStunImmune(true);
	}

	self->AddToGroup("boss");

	BaseEnemyApe::OnStartup(self);
}

void WaveBossApe::OnDie(Entity* self, Entity* killer) {
	BaseWavesGenericEnemy::OnDie(self, killer);
	BaseEnemyApe::OnDie(self, killer);
}

void WaveBossApe::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "startAI") {
		auto* combatAIComponent = self->GetComponent<BaseCombatAIComponent>();
		if (combatAIComponent != nullptr) {
			combatAIComponent->SetDisabled(false);
			combatAIComponent->SetStunImmune(false);
		}
	} else {
		BaseEnemyApe::OnFireEventServerSide(self, sender, args, param1, param2, param3);
	}
}
