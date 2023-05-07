#include "AgSurvivalSpiderling.h"
#include "BaseCombatAIComponent.h"

void AgSurvivalSpiderling::OnStartup(Entity* self) {
	BaseWavesGenericEnemy::OnStartup(self);

	auto* combatAI = self->GetComponent<BaseCombatAIComponent>();
	if (combatAI != nullptr) {
		combatAI->SetStunImmune(true);
	}
}

uint32_t AgSurvivalSpiderling::GetPoints() {
	return 300;
}
