#include "ImgBrickConsoleQB.h"
#include "RebuildComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"

int32_t ImgBrickConsoleQB::ResetBricks = 30;
int32_t ImgBrickConsoleQB::ResetConsole = 60;
int32_t ImgBrickConsoleQB::ResetInteract = 45;

void ImgBrickConsoleQB::OnStartup(Entity* self) 
{
    self->SetNetworkVar(u"used", false);

    self->AddTimer("reset", ResetBricks);
}

void ImgBrickConsoleQB::OnUse(Entity* self, Entity* user) 
{
    auto* rebuildComponent = self->GetComponent<RebuildComponent>();

    if (rebuildComponent->GetState() == REBUILD_COMPLETED)
    {
        if (!self->GetNetworkVar<bool>(u"used"))
        {
            const auto consoles = EntityManager::Instance()->GetEntitiesInGroup("Console");

            auto bothBuilt = false;

            for (auto* console : consoles)
            {
                auto* consoleRebuildComponent = console->GetComponent<RebuildComponent>();

                if (consoleRebuildComponent->GetState() != REBUILD_COMPLETED)
                {
                    continue;
                }

                console->CancelAllTimers();

                if (console->GetNetworkVar<bool>(u"used"))
                {
                    bothBuilt = true;
                }
            }

            if (bothBuilt)
            {
                SmashCanister(self);
            }
            else
            {
                SpawnBrick(self);
            }

            self->AddTimer("Die", ResetInteract);

            auto onFX = 0;

            const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

            if (location == "Left")
            {
                onFX = 2776;
            }
            else
            {
                onFX = 2779;
            }

            const auto& facility = EntityManager::Instance()->GetEntitiesInGroup("FacilityPipes");

            if (!facility.empty())
            {
                GameMessages::SendStopFXEffect(facility[0], true, location + "PipeEnergy");
                GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), onFX, u"create", location + "PipeOn");
            }
        }

        auto* player = user;

        auto* missionComponent = player->GetComponent<MissionComponent>();
        auto* inventoryComponent = player->GetComponent<InventoryComponent>();

        if (missionComponent != nullptr && inventoryComponent != nullptr)
        {
            if (missionComponent->GetMissionState(1302) == MissionState::MISSION_STATE_ACTIVE)
            {
                inventoryComponent->RemoveItem(13074, 1);
                
                missionComponent->ForceProgressTaskType(1302, 1, 1);
            }
            
            if (missionComponent->GetMissionState(1926) == MissionState::MISSION_STATE_ACTIVE)
            {
                inventoryComponent->RemoveItem(14472, 1);

                missionComponent->ForceProgressTaskType(1926, 1, 1);
            }
        }

        self->SetNetworkVar(u"used", true);

        GameMessages::SendTerminateInteraction(player->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
    }
}

void ImgBrickConsoleQB::SpawnBrick(Entity* self) 
{
    const auto netDevil = dZoneManager::Instance()->GetSpawnersByName("MaelstromBug");
    if (!netDevil.empty())
    {
        netDevil[0]->Reset();
        netDevil[0]->Deactivate();
    }

    const auto brick = dZoneManager::Instance()->GetSpawnersByName("Imagination");
    if (!brick.empty())
    {
        brick[0]->Activate();
    }
}

void ImgBrickConsoleQB::SmashCanister(Entity* self) 
{
    const auto brick = EntityManager::Instance()->GetEntitiesInGroup("Imagination");
    if (!brick.empty())
    {
        GameMessages::SendPlayFXEffect(brick[0]->GetObjectID(), 122, u"create", "bluebrick");
        GameMessages::SendPlayFXEffect(brick[0]->GetObjectID(), 1034, u"cast", "imaginationexplosion");
    }

    const auto canisters = EntityManager::Instance()->GetEntitiesInGroup("Canister");
    for (auto* canister : canisters)
    {
        canister->Smash(canister->GetObjectID(), VIOLENT);
    }
    
    const auto canister = dZoneManager::Instance()->GetSpawnersByName("BrickCanister");
    if (!canister.empty())
    {
        canister[0]->Reset();
        canister[0]->Deactivate();
    }
}

void ImgBrickConsoleQB::OnRebuildComplete(Entity* self, Entity* target) 
{
    auto energyFX = 0;

    const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

    if (location == "Left")
    {
        energyFX = 2775;
    }
    else
    {
        energyFX = 2778;
    }

    const auto& facility = EntityManager::Instance()->GetEntitiesInGroup("FacilityPipes");

    if (!facility.empty())
    {
        GameMessages::SendStopFXEffect(facility[0], true, location + "PipeOff");
        GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), energyFX, u"create", location + "PipeEnergy");
    }

    const auto consoles = EntityManager::Instance()->GetEntitiesInGroup("Console");

    for (auto* console : consoles)
    {
        auto* consoleRebuildComponent = console->GetComponent<RebuildComponent>();

        if (consoleRebuildComponent->GetState() != REBUILD_COMPLETED)
        {
            continue;
        }

        console->CancelAllTimers();
    }

    self->AddTimer("Die", ResetConsole);
}

void ImgBrickConsoleQB::OnDie(Entity* self, Entity* killer) 
{
    if (self->GetVar<bool>(u"Died"))
    {
        return;
    }

    self->CancelAllTimers();

    self->SetVar(u"Died", true);

    Game::logger->Log("ImgBrickConsoleQB", "On Die...\n");

    auto* rebuildComponent = self->GetComponent<RebuildComponent>();

    if (rebuildComponent->GetState() == REBUILD_COMPLETED)
    {
        auto offFX = 0;

        const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

        if (location == "Left")
        {
            offFX = 2774;
        }
        else
        {
            offFX = 2777;
        }

        const auto& facility = EntityManager::Instance()->GetEntitiesInGroup("FacilityPipes");

        if (!facility.empty())
        {
            GameMessages::SendStopFXEffect(facility[0], true, location + "PipeEnergy");
            GameMessages::SendStopFXEffect(facility[0], true, location + "PipeOn");
            GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), offFX, u"create", location + "PipeOff");
            GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), 2750, u"create", location + "imagination_canister");
        }
    }
    
    const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));
 
    const auto pipeGroup = myGroup.substr(0, 10);

    const auto firstPipe = pipeGroup + "1";

    const auto samePipeSpawner = dZoneManager::Instance()->GetSpawnersByName(myGroup);
    if (!samePipeSpawner.empty())
    {
        samePipeSpawner[0]->Reset();
        samePipeSpawner[0]->Deactivate();
    }
    
    const auto firstPipeSpawner = dZoneManager::Instance()->GetSpawnersByName(firstPipe);
    if (!firstPipeSpawner.empty())
    {
        firstPipeSpawner[0]->Activate();
    }

    const auto netdevil = dZoneManager::Instance()->GetSpawnersByName("Imagination");
    if (!netdevil.empty())
    {
        netdevil[0]->Reset();
        netdevil[0]->Deactivate();
    }

    const auto brick = dZoneManager::Instance()->GetSpawnersByName("MaelstromBug");
    if (!brick.empty())
    {
        brick[0]->Activate();
    }

    const auto canister = dZoneManager::Instance()->GetSpawnersByName("BrickCanister");
    if (!canister.empty())
    {
        canister[0]->Activate();
    }

    self->SetNetworkVar(u"used", false);

    Game::logger->Log("ImgBrickConsoleQB", "Died...\n");
}

void ImgBrickConsoleQB::OnTimerDone(Entity* self, std::string timerName) 
{
    if (timerName == "reset")
    {
        Game::logger->Log("ImgBrickConsoleQB", "Resetting...\n");

        auto* rebuildComponent = self->GetComponent<RebuildComponent>();

        if (rebuildComponent->GetState() == REBUILD_OPEN)
        {
            self->Smash(self->GetObjectID(), SILENT);
        }
    }
    else if (timerName == "Die")
    {
        Game::logger->Log("ImgBrickConsoleQB", "Die...\n");

        const auto consoles = EntityManager::Instance()->GetEntitiesInGroup("Console");

        for (auto* console : consoles)
        {
            console->Smash(console->GetObjectID(), VIOLENT);
        }
    }
}
