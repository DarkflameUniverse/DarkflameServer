#include "AgSurvivalBuffStation.h"
#include "SkillComponent.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
    auto destroyableComponent = self->GetComponent<DestroyableComponent>();
    
    if (destroyableComponent != nullptr) destroyableComponent->SetFaction(6);

    auto skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent == nullptr) return;

    skillComponent->CalculateBehavior(201, 1784, self->GetObjectID());

    self->AddCallbackTimer(10.0f, [self]() {
        self->Smash();
    });
}
