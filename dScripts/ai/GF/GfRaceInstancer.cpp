#include "GfRaceInstancer.h"

#include "Entity.h"

void GfRaceInstancer::OnStartup(Entity* self) {
	self->SetProximityRadius(self->HasVar(u"interaction_distance") ? self->GetVar<float>(u"interaction_distance") : 16.0f, "Interaction_Distance");
}
