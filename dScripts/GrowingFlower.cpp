#include "GrowingFlower.h"
#include "MissionComponent.h"

void GrowingFlower::OnSkillEventFired(Entity* self, Entity* target, const std::string& message) {
	if (!self->GetVar<bool>(u"blooming") && (message == "waterspray" || message == "shovelgrow")) {
		self->SetVar<bool>(u"blooming", true);
		self->SetNetworkVar(u"blooming", true);
		self->AddTimer("FlowerDie", GrowingFlower::aliveTime);

		const auto mission1 = self->GetVar<int32_t>(u"missionID");
		const auto mission2 = self->GetVar<int32_t>(u"missionID2");

		LootGenerator::Instance().DropActivityLoot(target, self, self->GetLOT(), 0);

		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			for (const auto mission : achievementIDs)
				missionComponent->ForceProgressTaskType(mission, static_cast<uint32_t>(MissionTaskType::MISSION_TASK_TYPE_SCRIPT), 1);

			if (mission1 && missionComponent->GetMissionState(mission1) == MissionState::MISSION_STATE_ACTIVE)
				missionComponent->ForceProgressTaskType(mission1, static_cast<uint32_t>(MissionTaskType::MISSION_TASK_TYPE_SCRIPT), 1);

			if (mission2 && missionComponent->GetMissionState(mission2) == MissionState::MISSION_STATE_ACTIVE)
				missionComponent->ForceProgressTaskType(mission2, static_cast<uint32_t>(MissionTaskType::MISSION_TASK_TYPE_SCRIPT), 1);
		}
	}
}

void GrowingFlower::OnTimerDone(Entity* self, std::string message) {
	if (message == "FlowerDie") {
		self->Smash();
	}
}

const std::vector<uint32_t> GrowingFlower::achievementIDs = { 143, 152, 153, 1409, 1507, 1544, 1581, 1845 };
