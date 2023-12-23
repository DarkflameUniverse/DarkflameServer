#include "WildSnake05.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "RenderComponent.h"

void WildSnake05::OnStartup(Entity* self) {
	self->AddTimer("SnakeStartup",0.2f);
	self->AddTimer("SnakeFollow",0.3f);
	self->AddTimer("SnakeGlow",2.0f);
}

void WildSnake05::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "SnakeStartup"){
		self->AddToGroup("snakesonaplane");
		auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
		if (destroyableComponent) destroyableComponent->SetFaction(4);
	} else if (timerName == "SnakeFollow") {
		auto snakes = Game::entityManager->GetEntitiesInGroup("snakesonaplane");
		for (const auto &snake : snakes) {
			if (snake && snake->GetLOT() == 6566){
				// self:FollowTarget { targetID = friends[i], radius = 4.5, speed = 1, keepFollowing = true }
			}
		}
	} else if (timerName == "SnakeGlow") {
		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent) renderComponent->PlayEffect(634, u"red", "");
	}
}

void WildSnake05::OnDie(Entity* self, Entity* killer) {
	auto snakes = Game::entityManager->GetEntitiesInGroup("snakesonaplane");
	for (const auto &snake : snakes) {
		if (snake) snake->NotifyObject(self, "next1");
	}
}
