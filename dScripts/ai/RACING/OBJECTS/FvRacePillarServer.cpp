#include "FvRacePillarServer.h"

#include "Game.h"
#include "EntityManager.h"
#include "RenderComponent.h"

void FvRacePillarServer::PlayAnimation(const std::string animName, const std::string group, const LOT lot) {
	const auto entities = Game::entityManager->GetEntitiesInGroup(group);
	for (const auto& entity : entities) {
		if (!entity || entity->GetLOT() != lot) continue;
		auto* renderComponent = entity->GetComponent<RenderComponent>();
		if (!renderComponent) continue;
		renderComponent->PlayAnimation(entity, animName);
	}
}
