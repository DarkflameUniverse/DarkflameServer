#include "PrSeagullFly.h"
#include "Entity.h"

void PrSeagullFly::OnStartup(Entity* self) {
	self->SetVar<int32_t>(u"playersNear", 0);
	self->SetProximityRadius(15, "birdMonitor");
}

void PrSeagullFly::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (!entering->IsPlayer()) return;

	if (status == "ENTER") {
		self->SetVar<int32_t>(u"playersNear", self->GetVar<int32_t>(u"playersNear") + 1);
	} else if (status == "LEAVE") {
		self->SetVar<int32_t>(u"playersNear", self->GetVar<int32_t>(u"playersNear") - 1);
	}

	self->SetNetworkVar(u"BirdLanded", self->GetVar<int32_t>(u"playersNear") == 0);
}
