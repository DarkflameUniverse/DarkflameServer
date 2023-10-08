#include "AgFans.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "PhantomPhysicsComponent.h"
#include "RenderComponent.h"
#include "eReplicaComponentType.h"
#include "RenderComponent.h"
#include "Entity.h"

void AgFans::OnStartup(Entity* self) {
	self->SetVar<bool>(u"alive", true);
	self->SetVar<bool>(u"on", false);

	ToggleFX(self, false);

	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(eReplicaComponentType::RENDER));

	if (renderComponent == nullptr) {
		return;
	}

	renderComponent->PlayEffect(495, u"fanOn", "fanOn");
}

void AgFans::ToggleFX(Entity* self, bool hit) {
	std::string fanGroup = self->GetGroups()[0];
	std::vector<Entity*> fanVolumes = Game::entityManager->GetEntitiesInGroup(fanGroup);

	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(eReplicaComponentType::RENDER));

	if (renderComponent == nullptr) {
		return;
	}

	if (fanVolumes.size() == 0 || !self->GetVar<bool>(u"alive")) return;

	if (self->GetVar<bool>(u"on")) {
		RenderComponent::PlayAnimation(self, u"fan-off");

		renderComponent->StopEffect("fanOn");
		self->SetVar<bool>(u"on", false);

		for (Entity* volume : fanVolumes) {
			PhantomPhysicsComponent* volumePhys = static_cast<PhantomPhysicsComponent*>(volume->GetComponent(eReplicaComponentType::PHANTOM_PHYSICS));
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(false);
			Game::entityManager->SerializeEntity(volume);
			if (!hit) {
				Entity* fxObj = Game::entityManager->GetEntitiesInGroup(fanGroup + "fx")[0];
				RenderComponent::PlayAnimation(fxObj, u"trigger");
			}
		}
	} else if (!self->GetVar<bool>(u"on") && self->GetVar<bool>(u"alive")) {
		RenderComponent::PlayAnimation(self, u"fan-on");

		renderComponent->PlayEffect(495, u"fanOn", "fanOn");
		self->SetVar<bool>(u"on", true);

		for (Entity* volume : fanVolumes) {
			PhantomPhysicsComponent* volumePhys = static_cast<PhantomPhysicsComponent*>(volume->GetComponent(eReplicaComponentType::PHANTOM_PHYSICS));
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(true);
			Game::entityManager->SerializeEntity(volume);
			if (!hit) {
				Entity* fxObj = Game::entityManager->GetEntitiesInGroup(fanGroup + "fx")[0];
				RenderComponent::PlayAnimation(fxObj, u"idle");
			}
		}
	}
}

void AgFans::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args.length() == 0 || !self->GetVar<bool>(u"alive")) return;

	if ((args == "turnOn" && self->GetVar<bool>(u"on")) || (args == "turnOff" && !self->GetVar<bool>(u"on"))) return;
	ToggleFX(self, false);
}

void AgFans::OnDie(Entity* self, Entity* killer) {
	if (self->GetVar<bool>(u"on")) {
		ToggleFX(self, true);
	}
	self->SetVar<bool>(u"alive", false);
}
