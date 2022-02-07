#include "AgSurvivalBuffStation.h"
#include "SkillComponent.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
    auto destroyableComponent = self->GetComponent<DestroyableComponent>();
    // We set the faction to 6 so that the buff station sees players as friendly targets
    if (destroyableComponent != nullptr) destroyableComponent->SetFaction(6);

    auto skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent == nullptr) return;

    skillComponent->CalculateBehavior(skillIdForBuffStation, behaviorIdForBuffStation, self->GetObjectID());

    self->AddCallbackTimer(10.0f, [self]() {
        self->Smash();
    });
}
