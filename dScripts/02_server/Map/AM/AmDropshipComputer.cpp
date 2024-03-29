#include "AmDropshipComputer.h"
#include "MissionComponent.h"
#include "QuickBuildComponent.h"
#include "InventoryComponent.h"
#include "dZoneManager.h"
#include "eMissionState.h"

void AmDropshipComputer::OnStartup(Entity* self) {
	self->AddTimer("reset", 45.0f);
}

void AmDropshipComputer::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (!quickBuildComponent || quickBuildComponent->GetState() != eQuickBuildState::COMPLETED) return;

	auto* missionComponent = user->GetComponent<MissionComponent>();
	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	if (!missionComponent || !inventoryComponent) return;

	if (inventoryComponent->GetLotCount(m_NexusTalonDataCard) != 0 || missionComponent->GetMission(979)->GetMissionState() == eMissionState::COMPLETE) {
		return;
	}

	inventoryComponent->AddItem(m_NexusTalonDataCard, 1, eLootSourceType::NONE);
}

void AmDropshipComputer::OnDie(Entity* self, Entity* killer) {
	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	const auto pipeNum = GeneralUtils::TryParse<int32_t>(myGroup.substr(10, 1));
	if (!pipeNum) return;

	const auto pipeGroup = myGroup.substr(0, 10);

	const auto nextPipeNum = pipeNum.value() + 1;

	const auto samePipeSpawners = Game::zoneManager->GetSpawnersByName(myGroup);

	if (!samePipeSpawners.empty()) {
		samePipeSpawners[0]->SoftReset();

		samePipeSpawners[0]->Deactivate();
	}

	if (killer != nullptr && killer->IsPlayer()) {
		const auto nextPipe = pipeGroup + std::to_string(nextPipeNum);

		const auto nextPipeSpawners = Game::zoneManager->GetSpawnersByName(nextPipe);

		if (!nextPipeSpawners.empty()) {
			nextPipeSpawners[0]->Activate();
		}
	} else {
		const auto nextPipe = pipeGroup + "1";

		const auto firstPipeSpawners = Game::zoneManager->GetSpawnersByName(nextPipe);

		if (!firstPipeSpawners.empty()) {
			firstPipeSpawners[0]->Activate();
		}
	}
}

void AmDropshipComputer::OnTimerDone(Entity* self, std::string timerName) {
	const auto* const quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (!quickBuildComponent) return;

	if (timerName == "reset" && quickBuildComponent->GetState() == eQuickBuildState::OPEN) {
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}
