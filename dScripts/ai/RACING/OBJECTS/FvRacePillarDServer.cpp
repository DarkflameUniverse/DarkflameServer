#include "FvRacePillarDServer.h"
#include "RenderComponent.h"
#include "RacingControlComponent.h"

void FvRacePillarDServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;
	const auto* racingController = racingControllers.at(0);

	auto* racingControlComponent = racingController->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;
	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if(!player) return;
	if (player->lap == 3){
				LOG("pillar d, Script");

		const auto pillars = Game::entityManager->GetEntitiesInGroup("pillars");
		for (const auto &pillar : pillars){
			if (!pillar || pillar->GetLOT() != this->m_PillarD) continue;
			auto* renderComponent = pillar->GetComponent<RenderComponent>();
			if (!renderComponent) continue;
			renderComponent->PlayAnimation(pillar, "crumble");
		}

		const auto dragons = Game::entityManager->GetEntitiesInGroup("dragon");
		for (const auto &dragon : dragons){
			if (!dragon || dragon->GetLOT() != this->m_Dragon) continue;
			auto* renderComponent = dragon->GetComponent<RenderComponent>();
			if (!renderComponent) continue;
			renderComponent->PlayAnimation(dragon, "roar");
		}
		Game::entityManager->DestroyEntity(self);
	}
}