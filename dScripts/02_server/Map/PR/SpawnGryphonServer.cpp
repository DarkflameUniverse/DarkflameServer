#include "SpawnGryphonServer.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void SpawnGryphonServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 12433);
	self->SetVar<std::string>(u"petType", "gryphon");
	self->SetVar<uint32_t>(u"maxPets", 2);
	self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
	self->SetVar<std::u16string>(u"spawnCinematic", u"SentinelPet");
}

void SpawnGryphonServer::OnUse(Entity* self, Entity* user) {
	auto* missionComponent = user->GetComponent<MissionComponent>();
	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	// Little extra for handling the case of the egg being placed the first time
	if (missionComponent != nullptr && inventoryComponent != nullptr
		&& missionComponent->GetMissionState(1391) == MissionState::MISSION_STATE_ACTIVE) {
		inventoryComponent->RemoveItem(12483, inventoryComponent->GetLotCount(12483));
		GameMessages::SendTerminateInteraction(user->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
		return;
	}

	SpawnPetBaseServer::OnUse(self, user);
}
