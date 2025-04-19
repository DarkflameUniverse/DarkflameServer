#include "ActSharkPlayerDeathTrigger.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "Entity.h"

void ActSharkPlayerDeathTrigger::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "achieve") {
		auto missionComponent = sender->GetComponent<MissionComponent>();
		if (!missionComponent) return;

		// This check is only needed because dlu doesnt have proper collision checks on rotated phantom physics
		if (sender->GetIsDead() || !sender->GetPlayerReadyForUpdates()) return; //Don't kill already dead players or players not ready
		
		missionComponent->Progress(eMissionTaskType::SCRIPT, 8419);

		if (sender->GetCharacter()) {
			sender->Smash(self->GetObjectID(), eKillType::VIOLENT, u"big-shark-death");
		}
	}
}
