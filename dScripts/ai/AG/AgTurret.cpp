#include "AgTurret.h"
#include "GameMessages.h"
#include "MovementMessages.h"

void AgTurret::OnStartup(Entity* self) {
	// TODO: do this legit way
	self->AddTimer("killTurret", 20.0f);
}

void AgTurret::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killTurret") {
		self->ScheduleKillAfterUpdate();
	}
}

void AgTurret::OnQuickBuildStart(Entity* self, Entity* user) {
	GameMessages::LockNodeRotation lockNodeRotation;
	lockNodeRotation.target = self->GetObjectID();
	lockNodeRotation.nodeName = "base";
	lockNodeRotation.Send(UNASSIGNED_SYSTEM_ADDRESS);
}
