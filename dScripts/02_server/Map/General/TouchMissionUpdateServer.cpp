#include "TouchMissionUpdateServer.h"

#include "Entity.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void TouchMissionUpdateServer::OnStartup(Entity* self) {
	self->SetProximityRadius(20, "touchCheck"); // Those does not have a collider for some reason?
}

void TouchMissionUpdateServer::OnCollisionPhantom(Entity* self, Entity* target) {
	int32_t missionId = self->GetVar<int32_t>(u"TouchCompleteID");

	if (missionId == 0) {
		return;
	}

	auto* missionComponent = static_cast<MissionComponent*>(target->GetComponent(COMPONENT_TYPE_MISSION));

	if (missionComponent == nullptr) {
		return;
	}

	auto* mission = missionComponent->GetMission(missionId);

	if (mission == nullptr) {
		return;
	}

	const auto state = mission->GetMissionState();

	if (state >= MissionState::MISSION_STATE_COMPLETE || mission->GetCompletions() > 1) {
		return;
	}

	for (auto* task : mission->GetTasks()) {
		if (!task->IsComplete()) {
			task->Complete();
		}
	}

	mission->CheckCompletion();
}

void TouchMissionUpdateServer::OnProximityUpdate(Entity* self, Entity* entering, const std::string name, const std::string status) {
	if (name != "touchCheck" || status != "ENTER") {
		return;
	}

	OnCollisionPhantom(self, entering);
}
