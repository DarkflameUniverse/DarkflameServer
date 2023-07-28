#include "AnvilOfArmor.h"

void AnvilOfArmor::OnStartup(Entity* self) {
	self->SetVar<uint32_t>(u"numCycles", 8);
	self->SetVar<float_t>(u"secPerCycle", 25.0f);
	self->SetVar<float_t>(u"delayToFirstCycle", 1.5f);
	self->SetVar<float_t>(u"deathDelay", 25.0f);
	self->SetVar<uint32_t>(u"numberOfPowerups", 4);
	self->SetVar<LOT>(u"lootLOT", 6431);

	// Initiate the actual script
	OnTemplateStartup(self);
}
