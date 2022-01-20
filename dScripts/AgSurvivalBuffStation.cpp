#include "AgSurvivalBuffStation.h"
#include "SkillComponent.h"
#include "dLogger.h"

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
    auto skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent == nullptr) return;

    skillComponent->CalculateBehavior(skillIdForBuffStation, behaviorIdForBuffStation, self->GetObjectID());
}
