#include "FvHorsemenTrigger.h"
#include "EntityManager.h"
#include "MissionComponent.h"

void FvHorsemenTrigger::OnStartup(Entity* self) 
{
    self->SetProximityRadius(40, "horsemenTrigger");

    self->SetVar<std::vector<LWOOBJID>>(u"players", {});
}

void FvHorsemenTrigger::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) 
{
    Game::logger->Log("FvHorsemenTrigger", "Proximity update\n");

    if (name != "horsemenTrigger" || !entering->IsPlayer())
    {
        return;
    }

    auto players = self->GetVar<std::vector<LWOOBJID>>(u"players");

    const auto& iter = std::find(players.begin(), players.end(), entering->GetObjectID());

    if (status == "ENTER" && iter == players.end())
    {
        Game::logger->Log("FvHorsemenTrigger", "Proximity enter\n");
        players.push_back(entering->GetObjectID());
    }
    else if (status == "LEAVE" && iter != players.end())
    {
        Game::logger->Log("FvHorsemenTrigger", "Proximity leave\n");
        players.erase(iter);
    }

    self->SetVar<std::vector<LWOOBJID>>(u"players", players);
}

void
FvHorsemenTrigger::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2,
                                         int32_t param3)
{
    auto players = self->GetVar<std::vector<LWOOBJID>>(u"players");

    Game::logger->Log("FvHorsemenTrigger", "Got event %s with %i players\n", args.c_str(), players.size());
    if (args == "HorsemenDeath")
    {
        for (const auto& playerId : self->GetVar<std::vector<LWOOBJID>>(u"players"))
        {
            auto* player = EntityManager::Instance()->GetEntity(playerId);

            if (player == nullptr)
            {
                continue;
            }

            auto* missionComponent = player->GetComponent<MissionComponent>();

            if (missionComponent == nullptr) return;

            missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
        }
    }
}
