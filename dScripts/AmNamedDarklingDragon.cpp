#include "AmNamedDarklingDragon.h"
#include "BaseCombatAIComponent.h"

void AmNamedDarklingDragon::OnStartup(Entity* self) 
{
    auto* baseCombatAIComponent = self->GetComponent<BaseCombatAIComponent>();

    if (baseCombatAIComponent != nullptr)
    {
        baseCombatAIComponent->SetStunImmune(true);
    }
}
