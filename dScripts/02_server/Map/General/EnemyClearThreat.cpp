#include "EnemyClearThreat.h"

#include "BaseCombatAIComponent.h"
#include "PhysicsComponent.h"

void EnemyClearThreat::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto colGroup = target->GetCollisionGroup();
	if (colGroup == 12) { // enemy
		auto* const baseCombatAiComponent = target->GetComponent<BaseCombatAIComponent>();
		if (!baseCombatAiComponent) return;

		baseCombatAiComponent->ClearThreat();
		baseCombatAiComponent->ForceTether();
	} else if (colGroup == 10) { // player
		const auto enemies = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::BASE_COMBAT_AI);
		for (const auto& enemy : enemies) {
			auto* const baseCombatAiComponent = enemy->GetComponent<BaseCombatAIComponent>();
			if (!baseCombatAiComponent) continue;

			baseCombatAiComponent->IgnoreThreat(target->GetObjectID(), 3.0f);
		}
	} 
}
