#include "GrowingFlower.h"
#include "MissionComponent.h"

void GrowingFlower::OnSkillEventFired(Entity *self, Entity *target, const std::string &message) {
    if (!self->GetVar<bool>(u"blooming") && (message == "waterspray" || message == "shovelgrow")) {
        self->SetVar<bool>(u"blooming", true);
        self->SetNetworkVar(u"blooming", true);
        self->AddTimer("FlowerDie", GrowingFlower::aliveTime);

        LootGenerator::Instance().DropActivityLoot(target, self, self->GetLOT());

        auto* missionComponent = target->GetComponent<MissionComponent>();

        if (missionComponent == nullptr) return;
        
        missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
    }
}

void GrowingFlower::OnTimerDone(Entity *self, std::string message) {
    if (message == "FlowerDie") {
        self->Smash();
    }
}