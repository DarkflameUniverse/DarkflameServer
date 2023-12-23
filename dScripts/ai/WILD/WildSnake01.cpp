#include "WildSnake01.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "RenderComponent.h"

void WildSnake01::OnStartup(Entity* self) {
	self->AddTimer("SnakeStartup",0.2f);
	self->AddTimer("SnakeFollow",0.3f);
}

void WildSnake01::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "next4"){
		auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
		if (destroyableComponent) destroyableComponent->SetFaction(4);
		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent) renderComponent->PlayEffect(634, u"purple", "");
	}
}

void WildSnake01::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "SnakeStartup"){
		self->AddToGroup("snakesonaplane");
	} else if (timerName == "SnakeFollow") {
		auto snakes = Game::entityManager->GetEntitiesInGroup("snakesonaplane");
		for (const auto &snake : snakes) {
			if (snake && snake->GetLOT() == 6602){
				// self:FollowTarget { targetID = friends[i], radius = 4.5, speed = 1, keepFollowing = true }
			}
		}
	}
}

void WildSnake01::OnDie(Entity* self, Entity* killer) {
	auto snakes = Game::entityManager->GetEntitiesInGroup("snakesonaplane");
	for (const auto &snake : snakes) {
		if (snake) snake->NotifyObject(self, "next5");
	}
}
