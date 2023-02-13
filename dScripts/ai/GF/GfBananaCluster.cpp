#include "GfBananaCluster.h"
#include "Entity.h"

void GfBananaCluster::OnStartup(Entity* self) {
	self->AddTimer("startup", 100);
}

void GfBananaCluster::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "startup") {
		self->ScheduleKillAfterUpdate(nullptr);
	}
}
