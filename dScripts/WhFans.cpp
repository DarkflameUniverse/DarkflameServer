#include "WhFans.h"

#include "RenderComponent.h"

void WhFans::OnStartup(Entity* self) {
	self->SetVar<bool>(u"alive", true);
	self->SetVar<bool>(u"on", false);

	ToggleFX(self, false);
}

void WhFans::ToggleFX(Entity* self, bool hit) {
	std::string fanGroup = "";
	try {
		fanGroup = self->GetGroups()[0];
	} catch(...) {
		fanGroup = "";
	}

	std::vector<Entity*> fanVolumes = EntityManager::Instance()->GetEntitiesInGroup(fanGroup);

	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));

	if (fanVolumes.size() == 0 || !self->GetVar<bool>(u"alive")) return;

	if (self->GetVar<bool>(u"on")) {
		GameMessages::SendPlayAnimation(self, u"fan-off");

		renderComponent->StopEffect("fanOn");
		self->SetVar<bool>(u"on", false);

		for (Entity* volume : fanVolumes) {
			auto volumePhys = volume->GetComponent<PhantomPhysicsComponent>();
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(false);
			EntityManager::Instance()->SerializeEntity(volume);
		}
	}
	else if (!self->GetVar<bool>(u"on") && self->GetVar<bool>(u"alive")) {
		GameMessages::SendPlayAnimation(self, u"fan-on");

		self->SetVar<bool>(u"on", true);

		for (Entity* volume : fanVolumes) {
			auto volumePhys = volume->GetComponent<PhantomPhysicsComponent>();
			if (!volumePhys) continue;
			volumePhys->SetPhysicsEffectActive(true);
			EntityManager::Instance()->SerializeEntity(volume);
		}
	}
}

void WhFans::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2,
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