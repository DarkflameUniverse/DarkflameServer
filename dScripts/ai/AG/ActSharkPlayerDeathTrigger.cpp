#include "ActSharkPlayerDeathTrigger.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "Entity.h"

void ActSharkPlayerDeathTrigger::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "achieve") {
		auto* missionComponent = sender->GetComponent<MissionComponent>();
		if (!missionComponent) return;

		missionComponent->Progress(eMissionTaskType::SCRIPT, 8419);

		if (sender->IsDead() || !sender->GetPlayerReadyForUpdates()) return; //Don't kill already dead players or players not ready

		if (sender->GetCharacter()) {
			sender->Smash(self->GetObjectID(), eKillType::VIOLENT, u"big-shark-death");
		}
	}
}
