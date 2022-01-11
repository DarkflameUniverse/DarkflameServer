#include "NjColeNPC.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"

void NjColeNPC::OnEmoteReceived(Entity* self, int32_t emote, Entity* target) 
{
    // Check if we used the "Dragon Roar" emote.
    if (emote != 393) return;

    auto* inventoryComponent = target->GetComponent<InventoryComponent>();

    if (inventoryComponent == nullptr) return;

    // Make sure we have a "Dragon Mask" equipped.
    if (!inventoryComponent->IsEquipped(14499) && !inventoryComponent->IsEquipped(16644)) return;

    auto* missionComponent = target->GetComponent<MissionComponent>();

    if (missionComponent == nullptr) return;

    missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
}

void NjColeNPC::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) 
{
    NjNPCMissionSpinjitzuServer::OnMissionDialogueOK(self, target, missionID, missionState);

    auto* missionComponent = target->GetComponent<MissionComponent>();

    if (missionComponent == nullptr) return;

    if (missionComponent->GetMissionState(missionID) != MissionState::MISSION_STATE_READY_TO_COMPLETE) return;
    
    auto* inventoryComponent = target->GetComponent<InventoryComponent>();

    if (inventoryComponent == nullptr) return;

    if (inventoryComponent->GetLotCount(14499) == 0) return;

    inventoryComponent->RemoveItem(14499, 1);
    inventoryComponent->AddItem(16644, 1);
}
