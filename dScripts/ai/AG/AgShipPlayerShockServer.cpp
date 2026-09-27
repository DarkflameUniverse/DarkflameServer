#include "AgShipPlayerShockServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "ObjectMessages.h"
#include "RenderComponent.h"
#include "Entity.h"
#include "eTerminateType.h"

void AgShipPlayerShockServer::OnUse(Entity* self, Entity* user) {
	GameMessages::TerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID()).Send(UNASSIGNED_SYSTEM_ADDRESS);
	if (active) {
		return;
	}
	active = true;
	RenderComponent::PlayAnimation(user, shockAnim);
	GameMessages::Knockback knockback;
	knockback.target = user->GetObjectID();
	knockback.Caster = self->GetObjectID();
	knockback.Originator = self->GetObjectID();
	knockback.vector = NiPoint3(-20, 10, -20);
	knockback.Send(UNASSIGNED_SYSTEM_ADDRESS);

	GameMessages::PlayFXEffect(self->GetObjectID(), 1430, u"create", "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->AddTimer("FXTime", fxTime);
}

void AgShipPlayerShockServer::OnTimerDone(Entity* self, std::string timerName) {
	GameMessages::StopFXEffect(self->GetObjectID(), true, "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);
	active = false;
}
