#include "SpecialImaginePowerupSpawner.h"

#include "GameMessages.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"

void SpecialImaginePowerupSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(1.5f, "powerupEnter");
	self->SetVar(u"bIsDead", false);
}

void SpecialImaginePowerupSpawner::OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) {
	if (name != "powerupEnter" && status != "ENTER") {
		return;
	}

	if (entering->GetLOT() != 1) {
		return;
	}

	if (self->GetVar<bool>(u"bIsDead")) {
		return;
	}

	GameMessages::SendPlayFXEffect(self, -1, u"pickup", "", LWOOBJID_EMPTY, 1, 1, true);

	SkillComponent* skillComponent;
	if (!self->TryGetComponent(COMPONENT_TYPE_SKILL, skillComponent)) {
		return;
	}

	const auto source = entering->GetObjectID();

	skillComponent->CalculateBehavior(13, 20, source);

	DestroyableComponent* destroyableComponent;
	if (!self->TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent)) {
		return;
	}

	self->SetVar(u"bIsDead", true);

	self->AddCallbackTimer(1.0f, [self]() {
		EntityManager::Instance()->ScheduleForKill(self);
		});
}
