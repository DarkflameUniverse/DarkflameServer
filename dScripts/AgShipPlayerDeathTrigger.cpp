#include "AgShipPlayerDeathTrigger.h"
#include "Entity.h"
#include "GameMessages.h"
#include "Game.h"
#include "dLogger.h"

void AgShipPlayerDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	if (target->GetLOT() == 1 && !target->GetIsDead()) {
		Game::logger->Log("CppScripts::AgShipPlayerDeathTrigger", "Attempting to kill %llu\n", target->GetObjectID());
		target->Smash(self->GetObjectID(), eKillType::VIOLENT, u"electro-shock-death");
	}
}