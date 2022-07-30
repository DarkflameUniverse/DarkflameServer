#include "AgDarkSpiderling.h"
#include "BaseCombatAIComponent.h"

void AgDarkSpiderling::OnStartup(Entity* self) {
	auto* combatAI = self->GetComponent<BaseCombatAIComponent>();
	if (combatAI != nullptr) {
		combatAI->SetStunImmune(true);
	}
}
