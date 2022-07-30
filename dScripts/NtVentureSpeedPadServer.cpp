#include "NtVentureSpeedPadServer.h"
#include "SkillComponent.h"
#include "MissionComponent.h"

void NtVentureSpeedPadServer::OnStartup(Entity* self) {
	self->SetProximityRadius(3, "speedboost");
}


void NtVentureSpeedPadServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "speedboost" || !entering->IsPlayer() || status != "ENTER") {
		return;
	}

	auto* player = entering;

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
	}

	auto* skillComponent = player->GetComponent<SkillComponent>();

	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(927, 18913, player->GetObjectID(), true);
	}
}
