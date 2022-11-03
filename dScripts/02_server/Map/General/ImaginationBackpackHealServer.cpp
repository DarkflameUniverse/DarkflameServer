#include "ImaginationBackpackHealServer.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void ImaginationBackpackHealServer::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message == "CastImaginationBackpack") {
		auto healMission = self->GetVar<int32_t>(u"FXOffMis");
		if (healMission == 0)
			healMission = self->GetVar<int32_t>(u"FXOnMis");
		if (healMission == 0)
			return;

		auto* missionComponent = caster->GetComponent<MissionComponent>();
		if (missionComponent != nullptr && missionComponent->GetMissionState(healMission) == MissionState::MISSION_STATE_ACTIVE) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"ClearMaelstrom", 0, 0,
				caster->GetObjectID(), "", caster->GetSystemAddress());
		}
	}
}
