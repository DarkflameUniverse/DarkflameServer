#include "FvMaelstromCavalry.h"
#include "EntityManager.h"

void FvMaelstromCavalry::OnStartup(Entity* self) 
{
    for (const auto& group : self->GetGroups())
    {
        Game::logger->Log("FvMaelstromCavalry", "Got group: %s\n", group.c_str());

        const auto& objects = EntityManager::Instance()->GetEntitiesInGroup(group);

        for (auto* obj : objects)
        {
            if (obj->GetLOT() != 8551) continue;

            Game::logger->Log("FvMaelstromCavalry", "Trigger in group: %s\n", group.c_str());

            obj->OnFireEventServerSide(self, "ISpawned");
        }
    }
}

void FvMaelstromCavalry::OnDie(Entity* self, Entity* killer) 
{
    if (killer == nullptr)
    {
        return;
    }

    Game::logger->Log("FvMaelstromCavalry", "Killer: %i\n", killer->GetLOT());

    if (killer->GetLOT() != 8665)
    {
        return;
    }

    const auto& triggers = EntityManager::Instance()->GetEntitiesInGroup("HorsemenTrigger");

    for (auto* trigger : triggers)
    {
        Game::logger->Log("FvMaelstromCavalry", "Trigger for: %i\n", killer->GetLOT());

        trigger->OnFireEventServerSide(self, "HorsemenDeath");
    }
}
