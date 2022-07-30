#include "AgShipPlayerShockServer.h"
#include "GameMessages.h"

void AgShipPlayerShockServer::OnUse(Entity* self, Entity* user) {
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
	if (active) {
		return;
	}
	active = true;
	GameMessages::SendPlayAnimation(user, shockAnim);
	GameMessages::SendKnockback(user->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, NiPoint3(-20, 10, -20));

	GameMessages::SendPlayFXEffect(self, 1430, u"create", "console_sparks", LWOOBJID_EMPTY, 1.0, 1.0, true);
	self->AddTimer("FXTime", fxTime);
}

void AgShipPlayerShockServer::OnTimerDone(Entity* self, std::string timerName) {
	GameMessages::SendStopFXEffect(self, true, "console_sparks");
	active = false;
}
