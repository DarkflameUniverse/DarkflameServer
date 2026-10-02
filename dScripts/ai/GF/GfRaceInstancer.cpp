#include "GfRaceInstancer.h"

#include "Entity.h"

void GfRaceInstancer::OnStartup(Entity* self) {
	self->SetProximityRadius(self->GetInteractionDistance(), "Interaction_Distance");
}
