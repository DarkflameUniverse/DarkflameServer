#include "AgSurvivalBuffStation.h"
#include "SkillComponent.h"
#include "dLogger.h"

void AgSurvivalBuffStation::OnStartup(Entity* self) {
    Game::logger->Log("AgSurvivalBuffStation", "Spawning survival buff station!\n");
}

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
    auto skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent == nullptr) return;

    skillComponent->CalculateBehavior(201, 1784, self->GetObjectID());
}
