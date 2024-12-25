#include "ActPlayerDeathTrigger.h"
#include "DestroyableComponent.h"
#include "Entity.h"

void ActPlayerDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer() || target->GetComponent<DestroyableComponent>()->GetIsDead() || !target->GetPlayerReadyForUpdates()) return; //Don't kill already dead players or players not ready

	target->Smash(self->GetObjectID(), eKillType::SILENT);
}
