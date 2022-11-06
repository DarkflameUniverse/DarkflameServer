#include "AgFans.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "PhantomPhysicsComponent.h"
#include "RenderComponent.h"

void AgFans::OnStartup(Entity* self) {
	self->SetVar<bool>(u"alive", true);
	self->SetVar<bool>(u"on", false);

	ToggleFX(self, false);

	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));

	if (renderComponent == nullptr) {
		return;
	}

	renderComponent->PlayEffect(495, u"fanOn", "fanOn");
}

void AgFans::ToggleFX(Entity* self, bool hit) {
	std::string fanGroup = self->GetGroups()[0];
	std::vector<Entity*> fanVolumes = EntityManager::Instance()->GetEntitiesInGroup(fanGroup);

	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));

	if (renderComponent == nullptr) {
		return;
	}

	if (fanVolumes.size() == 0 || !self->GetVar<bool>(u"alive")) return;

	if (self->GetVar<bool>(u"on")) {
		GameMessages::SendPlayAnimation(self, u"fan-off");

		renderComponent->StopEffect("fanOn");
		self->SetVar<bool>(u"on", false);

		for (Entity* volume : fanVolumes) {
			PhantomPhysicsComponent* volumePhys = static_cast<PhantomPhysicsComponent*>(volume->GetComponent(COMPONENT_TYPE_PHANTOM_PHYSICS));
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(false);
			EntityManager::Instance()->SerializeEntity(volume);
			if (!hit) {
				Entity* fxObj = EntityManager::Instance()->GetEntitiesInGroup(fanGroup + "fx")[0];
				GameMessages::SendPlayAnimation(fxObj, u"trigger");
			}
		}
	} else if (!self->GetVar<bool>(u"on") && self->GetVar<bool>(u"alive")) {
		GameMessages::SendPlayAnimation(self, u"fan-on");

		renderComponent->PlayEffect(495, u"fanOn", "fanOn");
		self->SetVar<bool>(u"on", true);

		for (Entity* volume : fanVolumes) {
			PhantomPhysicsComponent* volumePhys = static_cast<PhantomPhysicsComponent*>(volume->GetComponent(COMPONENT_TYPE_PHANTOM_PHYSICS));
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(true);
			EntityManager::Instance()->SerializeEntity(volume);
			if (!hit) {
				Entity* fxObj = EntityManager::Instance()->GetEntitiesInGroup(fanGroup + "fx")[0];
				GameMessages::SendPlayAnimation(fxObj, u"idle");
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
