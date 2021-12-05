#include "ClRing.h"
#include "EntityManager.h"
#include "Character.h"

void ClRing::OnCollisionPhantom(Entity* self, Entity* target) 
{
	self->Smash(target->GetObjectID());
}
