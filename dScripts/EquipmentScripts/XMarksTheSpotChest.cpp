#include "XMarksTheSpotChest.h"

#include "DestroyableComponent.h"
#include "Game.h"
#include "GameMessages.h"
#include "Loot.h"
#include "TeamManager.h"

    void
    XMarksTheSpotChest::OnStartup(Entity* self) {
    self->AddTimer("SpawnGoodies", 1);
}

void XMarksTheSpotChest::OnTimerDone(Entity* self, std::string timerName) {
    if (timerName == "SpawnGoodies") {

        const auto spawnPosition = self->GetPosition();

        const auto source = self->GetObjectID();

        const std::unordered_map<LOT, int32_t> result = std::unordered_map<LOT, int32_t>({{177, GeneralUtils::GenerateRandomNumber<uint32_t>(1, 10)},
                                                                                          {6431, GeneralUtils::GenerateRandomNumber<uint32_t>(1, 10)},
                                                                                          {935, GeneralUtils::GenerateRandomNumber<uint32_t>(1, 10)}});

        auto* entity = self->GetParentEntity();

        auto* team = TeamManager::Instance()->GetTeam(entity->GetObjectID());

        std::vector<LWOOBJID> entities = std::vector<LWOOBJID>();

        if (team)
            entities = team->members;
        else
            entities.push_back(entity->GetObjectID());

        for (const auto& objid : entities) {
            for (const auto& pair : result) {
                for (int i = 0; i < pair.second; ++i) {
                    GameMessages::SendDropClientLoot(Game::entityManager->GetEntity(objid), source, pair.first, 0, spawnPosition, 1);
                }
            }
        }

        self->AddTimer("Die", 1);
    } else if (timerName == "Die") {
        auto* destComp = self->GetComponent<DestroyableComponent>();
        if (destComp) {
            destComp->Smash(self->GetObjectID(), eKillType::SILENT);
        }
    }
}
