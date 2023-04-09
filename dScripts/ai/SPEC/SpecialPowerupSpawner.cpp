#include "SpecialPowerupSpawner.h"

#include "GameMessages.h"
#include "SkillComponent.h"
#include "EntityManager.h"
#include "eReplicaComponentType.h"

void SpecialPowerupSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(1.5f, "powerupEnter");
	self->SetVar(u"bIsDead", false);
}

void SpecialPowerupSpawner::OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) {
	if (name != "powerupEnter" && status != "ENTER") return;
	if (!entering->IsPlayer()) return;
	if (self->GetVar<bool>(u"bIsDead")) return;

	GameMessages::SendPlayFXEffect(self, -1, u"pickup", "", LWOOBJID_EMPTY, 1, 1, true);

	SkillComponent* skillComponent;
	if (!self->TryGetComponent(eReplicaComponentType::SKILL, skillComponent)) return;
	skillComponent->CastSkill(13, entering->GetObjectID());

	self->SetVar(u"bIsDead", true);
	self->Smash(entering->GetObjectID(), eKillType::SILENT);
}
