#include "AgShipPlayerDeathTrigger.h"
#include "DestroyableComponent.h"
#include "Entity.h"

void AgShipPlayerDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	if (target->GetLOT() == 1 && !target->GetComponent<DestroyableComponent>()->GetIsDead()) {
		target->Smash(self->GetObjectID(), eKillType::VIOLENT, u"electro-shock-death");
	}
}
