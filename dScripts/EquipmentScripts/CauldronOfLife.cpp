#include "CauldronOfLife.h"

void CauldronOfLife::OnStartup(Entity* self) {
	self->SetVar<uint32_t>(u"numCycles", 10);
	self->SetVar<float_t>(u"secPerCycle", 20.0f);
	self->SetVar<float_t>(u"delayToFirstCycle", 1.5f);
	self->SetVar<float_t>(u"deathDelay", 20.0f);
	self->SetVar<uint32_t>(u"numberOfPowerups", 3);
	self->SetVar<LOT>(u"lootLOT", 177);

	// Initiate the actual script
	OnTemplateStartup(self);
}
