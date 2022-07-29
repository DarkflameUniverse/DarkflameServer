#include "AmDropshipComputer.h"
#include "MissionComponent.h"
#include "RebuildComponent.h"
#include "InventoryComponent.h"
#include "dZoneManager.h"

void AmDropshipComputer::OnStartup(Entity* self) {
	self->AddTimer("reset", 45.0f);
}

void AmDropshipComputer::OnUse(Entity* self, Entity* user) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent == nullptr || rebuildComponent->GetState() != REBUILD_COMPLETED) {
		return;
	}

	auto* missionComponent = user->GetComponent<MissionComponent>();
	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	if (missionComponent == nullptr || inventoryComponent == nullptr) {
		return;
	}

	if (inventoryComponent->GetLotCount(m_NexusTalonDataCard) != 0 || missionComponent->GetMission(979)->GetMissionState() == MissionState::MISSION_STATE_COMPLETE) {
		return;
	}

	inventoryComponent->AddItem(m_NexusTalonDataCard, 1, eLootSourceType::LOOT_SOURCE_NONE);
}

void AmDropshipComputer::OnDie(Entity* self, Entity* killer) {
	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	int32_t pipeNum = 0;
	if (!GeneralUtils::TryParse<int32_t>(myGroup.substr(10, 1), pipeNum)) {
		return;
	}

	const auto pipeGroup = myGroup.substr(0, 10);

	const auto nextPipeNum = pipeNum + 1;

	const auto samePipeSpawners = dZoneManager::Instance()->GetSpawnersByName(myGroup);

	if (!samePipeSpawners.empty()) {
		samePipeSpawners[0]->SoftReset();

		samePipeSpawners[0]->Deactivate();
	}

	if (killer != nullptr && killer->IsPlayer()) {
		const auto nextPipe = pipeGroup + std::to_string(nextPipeNum);

		const auto nextPipeSpawners = dZoneManager::Instance()->GetSpawnersByName(nextPipe);

		if (!nextPipeSpawners.empty()) {
			nextPipeSpawners[0]->Activate();
		}
	} else {
		const auto nextPipe = pipeGroup + "1";

		const auto firstPipeSpawners = dZoneManager::Instance()->GetSpawnersByName(nextPipe);

		if (!firstPipeSpawners.empty()) {
			firstPipeSpawners[0]->Activate();
		}
	}
}

void AmDropshipComputer::OnTimerDone(Entity* self, std::string timerName) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent == nullptr) {
		return;
	}

	if (timerName == "reset" && rebuildComponent->GetState() == REBUILD_OPEN) {
		self->Smash(self->GetObjectID(), SILENT);
	}
}
