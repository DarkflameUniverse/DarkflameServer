#include "WhFans.h"

#include "RenderComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "PhantomPhysicsComponent.h"
#include "RenderComponent.h"
#include "Entity.h"

void WhFans::OnStartup(Entity* self) {
	self->SetVar<bool>(u"alive", true);
	self->SetVar<bool>(u"on", false);

	ToggleFX(self, false);
}

void WhFans::ToggleFX(Entity* self, bool hit) {
	std::string fanGroup;
	const auto& groups = self->GetGroups();
	if (!groups.empty()) {
		fanGroup = groups[0];
	} else {
		fanGroup = "";
	}

	std::vector<Entity*> fanVolumes = Game::entityManager->GetEntitiesInGroup(fanGroup);

	auto* renderComponent = self->GetComponent<RenderComponent>();

	if (renderComponent == nullptr) return;

	if (fanVolumes.size() == 0 || !self->GetVar<bool>(u"alive")) return;

	if (self->GetVar<bool>(u"on")) {
		RenderComponent::PlayAnimation(self, u"fan-off");

		renderComponent->StopEffect("fanOn");
		self->SetVar<bool>(u"on", false);

		for (Entity* volume : fanVolumes) {
			auto volumePhys = volume->GetComponent<PhantomPhysicsComponent>();
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(false);
			Game::entityManager->SerializeEntity(volume);
		}
	} else if (!self->GetVar<bool>(u"on") && self->GetVar<bool>(u"alive")) {
		RenderComponent::PlayAnimation(self, u"fan-on");

		self->SetVar<bool>(u"on", true);

		for (Entity* volume : fanVolumes) {
			auto volumePhys = volume->GetComponent<PhantomPhysicsComponent>();
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(true);
			Game::entityManager->SerializeEntity(volume);
		}
	}
}

void WhFans::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args.length() == 0 || !self->GetVar<bool>(u"alive")) return;

	if ((args == "turnOn" && self->GetVar<bool>(u"on")) || (args == "turnOff" && !self->GetVar<bool>(u"on"))) return;
	ToggleFX(self, false);
}

void WhFans::OnDie(Entity* self, Entity* killer) {
	if (self->GetVar<bool>(u"on")) {
		ToggleFX(self, true);
	}
	self->SetVar<bool>(u"alive", false);
}
