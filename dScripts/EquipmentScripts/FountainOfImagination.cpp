#include "FountainOfImagination.h"
#include "dCommonVars.h"
#include "Entity.h"

void FountainOfImagination::OnStartup(Entity* self) {
	self->SetVar<uint32_t>(u"numCycles", 6);
	self->SetVar<float_t>(u"secPerCycle", 30.0f);
	self->SetVar<float_t>(u"delayToFirstCycle", 1.5f);
	self->SetVar<float_t>(u"deathDelay", 30.0f);
	self->SetVar<uint32_t>(u"numberOfPowerups", 5);
	self->SetVar<LOT>(u"lootLOT", 935);

	// Initiate the actual script
	OnTemplateStartup(self);
}
