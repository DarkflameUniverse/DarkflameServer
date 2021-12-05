#include "AllCrateChicken.h"
#include "dCommonVars.h"
#include "EntityManager.h"
#include "Entity.h"

void AllCrateChicken::OnStartup(Entity* self) {
	self->AddTimer("KillRooster", 4.2f);
}

void AllCrateChicken::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "KillRooster") {
		self->ScheduleKillAfterUpdate();
	}
}
