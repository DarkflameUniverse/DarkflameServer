#include "FvRacePillarABCServer.h"
#include "RenderComponent.h"
#include "RacingControlComponent.h"

void FvRacePillarABCServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;

	auto* racingControlComponent = racingControllers[0]->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;

	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if (!player || player->lap != 1) return;

	PlayAnimation("crumble", "pillars", m_PillarA);
	PlayAnimation("roar", "dragon", m_Dragon);

	self->AddTimer("PillarBFall", 2.5f);
	self->AddTimer("PillarCFall", 3.7f);
	self->AddTimer("DeleteObject", 3.8f);
}

void FvRacePillarABCServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "PillarBFall") {
		PlayAnimation("crumble", "pillars", m_PillarB);
	} else if (timerName == "PillarCFall") {
		PlayAnimation("crumble", "pillars", m_PillarC);
	} else if (timerName == "DeleteObject") {
		Game::entityManager->DestroyEntity(self);
	}
}

