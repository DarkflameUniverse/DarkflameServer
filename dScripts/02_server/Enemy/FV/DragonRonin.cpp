#include "DragonRonin.h"

void DragonRonin::OnStartup(Entity* self) {
	self->SetVar<float>(u"suicideTimer", 40);
	CountdownDestroyAI::OnStartup(self);
}
