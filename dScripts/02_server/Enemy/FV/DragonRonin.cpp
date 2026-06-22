#include "DragonRonin.h"

void DragonRonin::OnStartup(Entity* self) {
	self->SetVar<float>(u"suicideTimer", 40.0f);
	CountdownDestroyAI::OnStartup(self);
}
