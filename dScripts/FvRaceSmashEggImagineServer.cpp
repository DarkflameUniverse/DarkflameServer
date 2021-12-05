#include "FvRaceSmashEggImagineServer.h"
#include "DestroyableComponent.h"
#include "CharacterComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"

void FvRaceSmashEggImagineServer::OnDie(Entity *self, Entity *killer) {
    if (killer != nullptr) {
        auto* destroyableComponent = killer->GetComponent<DestroyableComponent>();
        if (destroyableComponent != nullptr) {
            destroyableComponent->SetImagination(destroyableComponent->GetImagination() + 10);
            EntityManager::Instance()->SerializeEntity(killer);
        }

        // Crate is killed by the car
        auto* possessableComponent = killer->GetComponent<PossessableComponent>();
        if (possessableComponent != nullptr) {

            auto* possessor = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());
            if (possessor != nullptr) {

                auto* characterComponent = possessor->GetComponent<CharacterComponent>();
                if (characterComponent != nullptr) {
                    characterComponent->UpdatePlayerStatistic(ImaginationPowerUpsCollected);
                    characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
                }
            }
        }

    }
}
