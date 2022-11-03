#include "Sunflower.h"
#include "Entity.h"

void Sunflower::OnStartup(Entity* self) {
	self->SetVar<uint32_t>(u"numCycles", 6);
	self->SetVar<float_t>(u"secPerCycle", 5.0f);
	self->SetVar<float_t>(u"delayToFirstCycle", 1.5f);
	self->SetVar<float_t>(u"deathDelay", 30.0f);
	self->SetVar<uint32_t>(u"numberOfPowerups", 4);
	self->SetVar<LOT>(u"lootLOT", 11910);

	// Initiate the actual script
	OnTemplateStartup(self);
}
