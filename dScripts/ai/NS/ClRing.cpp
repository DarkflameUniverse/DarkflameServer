#include "ClRing.h"

void ClRing::OnCollisionPhantom(Entity* self, Entity* target) {
	self->Smash(target->GetObjectID());
}
