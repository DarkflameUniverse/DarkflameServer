#include "QbEnemyStunner.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"

void QbEnemyStunner::OnRebuildComplete(Entity* self, Entity* target)
{
    auto* destroyable = self->GetComponent<DestroyableComponent>();

    if (destroyable != nullptr)
    {
        destroyable->SetFaction(115);
    }

    auto* skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent != nullptr)
    {
        skillComponent->CalculateBehavior(499, 6095, LWOOBJID_EMPTY);
    }

    self->AddTimer("TickTime", 1);

    self->AddTimer("PlayEffect", 20);
}

void QbEnemyStunner::OnTimerDone(Entity* self, std::string timerName) 
{
    if (timerName == "DieTime")
    {
        self->Smash();

        self->CancelAllTimers();
    }
    else if (timerName == "PlayEffect")
    {
        self->SetNetworkVar(u"startEffect", 5.0f, UNASSIGNED_SYSTEM_ADDRESS);

        self->AddTimer("DieTime", 5.0f);
    }
    else if (timerName == "TickTime")
    {
        auto* skillComponent = self->GetComponent<SkillComponent>();

        if (skillComponent != nullptr)
        {
            skillComponent->CalculateBehavior(499, 6095, LWOOBJID_EMPTY);
        }
        
        self->AddTimer("TickTime", 1);
    }
}
