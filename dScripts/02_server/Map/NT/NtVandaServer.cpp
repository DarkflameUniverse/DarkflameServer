#include "NtVandaServer.h"
#include "InventoryComponent.h"
#include "eMissionState.h"
#include "MissionComponent.h"
#include "Character.h"
#include "ePlayerFlag.h"

void NtVandaServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	// Removes the alien parts after completing the mission
	if (missionID == m_AlienPartMissionID && missionState == eMissionState::READY_TO_COMPLETE) {
		auto* inventoryComponent = target->GetComponent<InventoryComponent>();
		for (const auto& alienPartLot : m_AlienPartLots) {
			inventoryComponent->RemoveItem(alienPartLot, 1);
		}
	}
	NtBcSubmitServer::OnMissionDialogueOK(self, target, missionID, missionState);
}


///////////////////////////////////////////////////////
//	For players who built imagimeter before 1.11	//
/////////////////////////////////////////////////////
void NtVandaServer::OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {	
	auto* missionComponent = player->GetComponent<MissionComponent>();
	auto* character = player->GetCharacter();
	
	if (missionID != 2088) return;		
	if (!missionComponent) return;

	missionComponent->ResetMission(1670);	
	missionComponent->ResetMission(1719);
	missionComponent->RemoveMission(1670);	
	missionComponent->RemoveMission(1719);	

	if (character) {
		character->SetPlayerFlag(1919, false);
		character->SetPlayerFlag(ePlayerFlag::NT_PLINTH_REBUILD, false);
	}		
}