#include "NsTokenConsoleServer.h"
#include "InventoryComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Character.h"
#include "MissionComponent.h"
#include "RebuildComponent.h"

void NsTokenConsoleServer::OnStartup(Entity* self) 
{
    
}

void NsTokenConsoleServer::OnUse(Entity* self, Entity* user) 
{
    auto* rebuildComponent = self->GetComponent<RebuildComponent>();

    if (rebuildComponent == nullptr)
    {
        return;
    }

    if (rebuildComponent->GetState() != REBUILD_COMPLETED)
    {
        return;
    }

    auto* inventoryComponent = user->GetComponent<InventoryComponent>();
    auto* missionComponent = user->GetComponent<MissionComponent>();
    auto* character = user->GetCharacter();

    if (inventoryComponent == nullptr || missionComponent == nullptr || character == nullptr)
    {
        return;
    }

    if (inventoryComponent->GetLotCount(6194) < 25)
    {
        return;
    }

    inventoryComponent->RemoveItem(6194, 25);

    const auto useSound = self->GetVar<std::string>(u"sound1");

    if (!useSound.empty())
    {
        GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, useSound);
    }

    if (character->GetPlayerFlag(46))
    {
        inventoryComponent->AddItem(8321, 5);
    }
    else if (character->GetPlayerFlag(47))
    {
        inventoryComponent->AddItem(8318, 5);
    }
    else if (character->GetPlayerFlag(48))
    {
        inventoryComponent->AddItem(8320, 5);
    }
    else if (character->GetPlayerFlag(49))
    {
        inventoryComponent->AddItem(8319, 5);
    }

    missionComponent->ForceProgressTaskType(863, 1, 1, false);
    
    GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}
