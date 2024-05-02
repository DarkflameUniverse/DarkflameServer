#include "FvRacePillarDServer.h"
#include "RenderComponent.h"
#include "RacingControlComponent.h"

void FvRacePillarDServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;

	auto* racingControlComponent = racingControllers[0]->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;

	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if (!player) return;

	if (player->lap == 2) {
		PlayAnimation("crumble", "pillars", m_PillarD);
		PlayAnimation("roar", "dragon", m_Dragon);
	}
}
