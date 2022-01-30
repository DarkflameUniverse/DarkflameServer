#include "ActSharkPlayerDeathTrigger.h"
#include "Entity.h"
#include "GameMessages.h"
#include "Game.h"
#include "dLogger.h"

void ActSharkPlayerDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {

}

void ActSharkPlayerDeathTrigger::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1,
                                                       int32_t param2, int32_t param3) {
	if (args == "achieve") {
		MissionComponent* mis = static_cast<MissionComponent*>(sender->GetComponent(COMPONENT_TYPE_MISSION));
		if (!mis) return;

		mis->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, 8419);

		if (sender->GetIsDead() || !sender->GetPlayerReadyForUpdates()) return; //Don't kill already dead players or players not ready
		
		Game::logger->Log("ActSharkPlayerDeathTrigger", "%i\n", self->GetLOT());

		if (sender->GetCharacter()) {
			sender->Smash(self->GetObjectID(), eKillType::VIOLENT, u"big-shark-death");
		}
	}
}
