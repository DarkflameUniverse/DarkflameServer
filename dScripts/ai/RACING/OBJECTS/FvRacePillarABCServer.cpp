#include "FvRacePillarABCServer.h"
#include "RenderComponent.h"
#include "RacingControlComponent.h"

void FvRacePillarABCServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;
	const auto* racingController = racingControllers.at(0);

	auto* racingControlComponent = racingController->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;
	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if(!player) return;
	if (player->lap == 2){
		LOG("pillar abcScript");
		const auto pillars = Game::entityManager->GetEntitiesInGroup("pillars");
		for (const auto &pillar : pillars){
			if (!pillar || pillar->GetLOT() != this->m_PillarA) continue;
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
		self->AddTimer("PillarBFall", 2.5);
		self->AddTimer("PillarCFall", 3.7);
		self->AddTimer("DeleteObject", 3.8);
	}
}

void FvRacePillarABCServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "PillarBFall"){
				LOG("pillar abcScriptbbbbbbbbbbbbbbbbbb");

		const auto pillars = Game::entityManager->GetEntitiesInGroup("pillars");
		for (const auto &pillar : pillars){
			if (!pillar || pillar->GetLOT() != this->m_PillarB) continue;
			auto* renderComponent = pillar->GetComponent<RenderComponent>();
			if (!renderComponent) continue;
			renderComponent->PlayAnimation(pillar, "crumble");
		}
	} else if (timerName == "PillarCFall") {
				LOG("pillar abcScriptcccccccccccccccccccccc");

		const auto pillars = Game::entityManager->GetEntitiesInGroup("pillars");
		for (const auto &pillar : pillars){
			if (!pillar || pillar->GetLOT() != this->m_PillarC) continue;
			auto* renderComponent = pillar->GetComponent<RenderComponent>();
			if (!renderComponent) continue;
			renderComponent->PlayAnimation(pillar, "crumble");
		}
	} else if (timerName == "DeleteObject") {
		Game::entityManager->DestroyEntity(self);
	}
}

