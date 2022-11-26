#include "AgMonumentBirds.h"
#include "GameMessages.h"

//--------------------------------------------------------------
//Makes the ag birds fly away when you get close and smashes them.
//Created mrb... 6 / 3 / 11
//Ported Max 20/07/2020
//--------------------------------------------------------------

void AgMonumentBirds::OnStartup(Entity* self) {
	self->SetProximityRadius(flyRadius, "MonumentBirds");
}

void AgMonumentBirds::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (self->GetVar<bool>(u"IsFlying")) return;

	if (name == "MonumentBirds" && status == "ENTER") {
		self->AddTimer("killBird", 1.0f);
		GameMessages::SendPlayAnimation(self, sOnProximityAnim);
		self->SetVar<bool>(u"IsFlying", true);
		self->SetVar<LWOOBJID>(u"PlayerID", entering->GetObjectID());
	}
}

void AgMonumentBirds::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "killBird") return;

	auto* player = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"PlayerID"));

	if (player == nullptr) return;

	self->ScheduleKillAfterUpdate(player);
}
