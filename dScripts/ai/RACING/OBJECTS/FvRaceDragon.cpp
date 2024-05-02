#include "FvRaceDragon.h"
#include "RenderComponent.h"
#include "RacingControlComponent.h"

void FvRaceDragon::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;

	auto* racingControlComponent = racingControllers[0]->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;

	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if (!player) return;

	if (player->lap != m_Lap) return;

	const auto dragons = Game::entityManager->GetEntitiesInGroup("dragon");
	for (const auto& dragon : dragons) {
		if (!dragon || dragon->GetLOT() != this->m_Dragon) continue;

		auto* renderComponent = dragon->GetComponent<RenderComponent>();
		if (!renderComponent) continue;

		renderComponent->PlayAnimation(dragon, m_LapAnimName);
		
	}
	Game::entityManager->DestroyEntity(self);
}
