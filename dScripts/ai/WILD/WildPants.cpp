#include "WildPants.h"
#include "GameMessages.h"

void WildPants::OnStartup(Entity* self) {
	self->SetProximityRadius(5, "scardyPants");
}

void WildPants::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status == "ENTER") GameMessages::SendPlayAnimation(self, u"scared");
}
