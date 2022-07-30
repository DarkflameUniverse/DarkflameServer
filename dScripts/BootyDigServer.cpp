#include "BootyDigServer.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "MissionComponent.h"
#include "MissionTaskType.h"

void BootyDigServer::OnStartup(Entity* self) {
	auto* zoneControlObject = EntityManager::Instance()->GetZoneControlEntity();
	if (zoneControlObject != nullptr) {
		zoneControlObject->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void BootyDigServer::OnPlayerLoaded(Entity* self, Entity* player) {
	auto* zoneControlObject = EntityManager::Instance()->GetZoneControlEntity();
	if (zoneControlObject != nullptr) {
		zoneControlObject->OnFireEventServerSide(self, "CheckForPropertyOwner");
	}
}

void
BootyDigServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {

	auto propertyOwner = self->GetNetworkVar<std::string>(u"PropertyOwnerID");
	auto* player = self->GetParentEntity();
	if (player == nullptr)
		return;

	if (args == "ChestReady" && (propertyOwner == std::to_string(LWOOBJID_EMPTY) || player->GetVar<bool>(u"bootyDug"))) {
		self->Smash(self->GetObjectID(), SILENT);
	} else if (args == "ChestOpened") {
		// Make sure players only dig up one booty per instance
		player->SetVar<bool>(u"bootyDug", true);

		auto* missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			auto* mission = missionComponent->GetMission(1881);
			if (mission != nullptr && (mission->GetMissionState() == MissionState::MISSION_STATE_ACTIVE || mission->GetMissionState() == MissionState::MISSION_STATE_COMPLETE_ACTIVE)) {
				mission->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());

				auto* renderComponent = self->GetComponent<RenderComponent>();
				if (renderComponent != nullptr)
					renderComponent->PlayEffect(7730, u"cast", "bootyshine");

				LootGenerator::Instance().DropLoot(player, self, 231, 75, 75);
			}
		}
	} else if (args == "ChestDead") {
		self->Smash(player->GetObjectID(), SILENT);
	}
}
