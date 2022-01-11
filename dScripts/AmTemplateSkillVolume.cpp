#include "AmTemplateSkillVolume.h"
#include "MissionComponent.h"

void AmTemplateSkillVolume::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) 
{
    if (message != "NinjagoSpinAttackEvent") return;

    auto* missionComponent = caster->GetComponent<MissionComponent>();

    if(missionComponent == nullptr) return;

    std::cout << self->GetLOT() << std::endl;
    missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
}
