#include "RaceImagineCrateServer.h"
#include "SkillComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "CharacterComponent.h"
#include "PossessableComponent.h"

void RaceImagineCrateServer::OnDie(Entity* self, Entity* killer)
{
    if (self->GetVar<bool>(u"bIsDead"))
    {
        return;
    }

    //GameMessages::SendPlayFXEffect(self, -1, u"pickup", "", LWOOBJID_EMPTY, 1, 1, true);

    self->SetVar<bool>(u"bIsDead", true);

    if (killer == nullptr)
    {
        return;
    }

    auto* skillComponent = killer->GetComponent<SkillComponent>();

    if (skillComponent == nullptr)
    {
        return;
    }

    auto* destroyableComponent = killer->GetComponent<DestroyableComponent>();

    if (destroyableComponent != nullptr)
    {
        destroyableComponent->SetImagination(60);

        EntityManager::Instance()->SerializeEntity(killer);
    }

    // Crate is killed by the car
    auto* possessableComponent = killer->GetComponent<PossessableComponent>();
    if (possessableComponent != nullptr) {

        auto* possessor = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());
        if (possessor != nullptr) {

            auto* characterComponent = possessor->GetComponent<CharacterComponent>();
            if (characterComponent != nullptr) {
                characterComponent->UpdatePlayerStatistic(RacingImaginationCratesSmashed);
                characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
            }
        }
    }


    //skillComponent->CalculateBehavior(586, 9450, killer->GetObjectID(), true);
}
