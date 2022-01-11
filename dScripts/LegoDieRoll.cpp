#include "LegoDieRoll.h"
#include "Entity.h"
#include "dLogger.h"
#include "GameMessages.h"

void LegoDieRoll::OnStartup(Entity* self) {
    self->AddTimer("DoneRolling", 10.0f);
    self->AddTimer("ThrowDice", LegoDieRoll::animTime);
}

void LegoDieRoll::OnTimerDone(Entity* self, std::string timerName) {
    if (timerName == "DoneRolling") {
        self->Smash(self->GetObjectID(), SILENT);
    } 
    else if (timerName == "ThrowDice") {
        int32_t dieRoll = GeneralUtils::GenerateRandomNumber<int32_t>(1, 6);
        GameMessages::SendPlayAnimation(self, anims[dieRoll - 1]);
        if (dieRoll == 6) {
            auto* user = self->GetOwner();
            auto* missionComponent = user->GetComponent<MissionComponent>();
            if(missionComponent == nullptr) return;

            missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
        }
    }
}