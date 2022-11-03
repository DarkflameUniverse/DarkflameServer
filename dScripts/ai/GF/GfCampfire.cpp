#include "GfCampfire.h"
#include "RenderComponent.h"
#include "SkillComponent.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "EntityManager.h"

void GfCampfire::OnStartup(Entity* self) {
	self->SetI32(u"counter", static_cast<int32_t>(0));
	self->SetProximityRadius(2.0f, "placeholder");
	self->SetBoolean(u"isBurning", true);

	auto* render = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));
	if (render == nullptr)
		return;

	render->PlayEffect(295, u"running", "Burn");
}

void GfCampfire::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "physicsReady") {
		auto* render = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));

		render->PlayEffect(295, u"running", "Burn");
	}
}

void GfCampfire::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* skill = self->GetComponent<SkillComponent>();

	if (self->GetBoolean(u"isBurning")) {
		if (status == "ENTER") {
			if (entering->GetCharacter()) {
				int32_t counter = self->GetI32(u"counter");
				counter = counter + 1;
				self->SetI32(u"counter", counter);

				if (counter == 1) {
					skill->CalculateBehavior(m_skillCastId, 115, entering->GetObjectID());
					self->AddTimer("TimeBetweenCast", FIRE_COOLDOWN);

					//self->SetVar<LWOOBJID>("target", entering->GetObjectID());

					auto* missionComponet = entering->GetComponent<MissionComponent>();

					if (missionComponet != nullptr) {
						missionComponet->ForceProgress(440, 658, 1);
					}
				}
			}
		} else {
			int32_t counter = self->GetI32(u"counter");
			if (counter > 0) {
				counter = counter - 1;
				self->SetI32(u"counter", counter);
				if (counter == 0) {
					self->CancelAllTimers();
				}
			}
		}
	}
}

void GfCampfire::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message == "waterspray" && self->GetVar<bool>(u"isBurning")) {
		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent != nullptr) {
			renderComponent->StopEffect("Burn");
			renderComponent->PlayEffect(295, u"idle", "Off");

			self->SetVar<bool>(u"isBurning", false);
			self->AddTimer("FireRestart", 37);
		}
	}
}

void GfCampfire::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "TimeBetweenCast") {
		/*
		self->AddTimer("TimeBetweenCast", FIRE_COOLDOWN);

		const auto targetId = self->GetVar<LWOOBJID>("target");

		auto* entering = EntityManager::Instance()->GetEntity(targetId);

		if (entering == nullptr)
		{

		}
		*/
	} else if (timerName == "FireRestart" && !self->GetVar<bool>(u"isBurning")) {
		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent != nullptr) {
			renderComponent->StopEffect("Off");
			renderComponent->PlayEffect(295, u"running", "Burn");
			self->SetVar<bool>(u"isBurning", true);
		}
	}
}
