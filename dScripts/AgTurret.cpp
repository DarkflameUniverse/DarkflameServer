#include "AgTurret.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "GameMessages.h"

void AgTurret::OnStartup(Entity* self) {
	// TODO: do this legit way
	self->AddTimer("killTurret", 20.0f);
}

void AgTurret::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killTurret") {
		self->ScheduleKillAfterUpdate();
	}
}

void AgTurret::OnRebuildStart(Entity* self, Entity* user) {
	GameMessages::SendLockNodeRotation(self, "base");
}
