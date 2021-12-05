#include "RaceSmashServer.h"
#include "CharacterComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"

void RaceSmashServer::OnDie(Entity *self, Entity *killer) {
    // Crate is smashed by the car
    auto* possessableComponent = killer->GetComponent<PossessableComponent>();
    if (possessableComponent != nullptr) {

        auto* possessor = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());
        if (possessor != nullptr) {

            auto* characterComponent = possessor->GetComponent<CharacterComponent>();
            if (characterComponent != nullptr) {
                characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
            }
        }
    }
}
