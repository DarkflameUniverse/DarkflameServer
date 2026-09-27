#include "AgShipPlayerShockServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "RenderComponent.h"
#include "Entity.h"
#include "eTerminateType.h"

void AgShipPlayerShockServer::OnUse(Entity* self, Entity* user) {
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
	if (active) {
		return;
	}
	active = true;
	RenderComponent::PlayAnimation(user, shockAnim);
	GameMessages::SendKnockback(user->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, NiPoint3(-20, 10, -20));

	GameMessages::PlayFXEffect(self->GetObjectID(), 1430, u"create", "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->AddTimer("FXTime", fxTime);
}

void AgShipPlayerShockServer::OnTimerDone(Entity* self, std::string timerName) {
	GameMessages::StopFXEffect(self->GetObjectID(), true, "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);
	active = false;
}
