#include "NjhubLavaPlayerDeathTrigger.h"
#include "Entity.h"

void NjhubLavaPlayerDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer())
		return;

	target->Smash(self->GetObjectID(), VIOLENT, u"drown");
}
