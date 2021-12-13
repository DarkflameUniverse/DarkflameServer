#include "MinigameTreasureChestServer.h"
#include "ScriptedActivityComponent.h"
#include "TeamManager.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "dServer.h"

void MinigameTreasureChestServer::OnUse(Entity *self, Entity *user) {
    auto* sac = self->GetComponent<ScriptedActivityComponent>();
    if (sac == nullptr)
        return;

    if (self->GetVar<bool>(u"used"))
        return;
    self->SetVar<bool>(u"used", true);

    if (!IsPlayerInActivity(self, user->GetObjectID()))
        UpdatePlayer(self, user->GetObjectID());

    auto* team = TeamManager::Instance()->GetTeam(user->GetObjectID());
    if (team != nullptr) {
        for (const auto& teamMemberID : team->members) {
            auto* teamMember = EntityManager::Instance()->GetEntity(teamMemberID);
            if (teamMember != nullptr)
                LootGenerator::Instance()->DropActivityLoot(teamMember, self, sac->GetActivityID(), CalculateActivityRating(self, teamMemberID));
        }
    } else {
        LootGenerator::Instance()->DropActivityLoot(user, self, sac->GetActivityID(), CalculateActivityRating(self, user->GetObjectID()));
    }

    sac->PlayerRemove(user->GetObjectID());

    auto* zoneControl = dZoneManager::Instance()->GetZoneControlObject();
    if (zoneControl != nullptr) {
        zoneControl->OnFireEventServerSide(self, "Survival_Update", 0);
    }

    self->Smash(self->GetObjectID());
}

uint32_t MinigameTreasureChestServer::CalculateActivityRating(Entity *self, LWOOBJID playerID) {
    auto* team = TeamManager::Instance()->GetTeam(playerID);
    return team != nullptr ? team->members.size() * 100 : ActivityManager::CalculateActivityRating(self, playerID) * 100;
}

void MinigameTreasureChestServer::OnStartup(Entity *self) {

    // BONS treasure chest thinks it's on FV, causing it to start a lobby
    if (Game::server->GetZoneID() == 1204) {
        auto* sac = self->GetComponent<ScriptedActivityComponent>();
        if (sac != nullptr) {
            sac->SetInstanceMapID(1204);
        }
    }
}
