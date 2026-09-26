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

	// Only players on the mission get the data card, and only once.
	if (missionComponent->GetMissionState(979) != eMissionState::ACTIVE || inventoryComponent->GetLotCount(m_NexusTalonDataCard) != 0) {
		return;
	}

	inventoryComponent->AddItem(m_NexusTalonDataCard, 1, eLootSourceType::NONE);
}

void AmDropshipComputer::OnDie(Entity* self, Entity* killer) {
	// Reset this computer's spawner network and start the first one again, however it died.
	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));
	const auto pipeGroup = myGroup.substr(0, 10);

	const auto samePipeSpawners = Game::zoneManager->GetSpawnersByName(myGroup);

	if (!samePipeSpawners.empty()) {
		samePipeSpawners[0]->SoftReset();

		samePipeSpawners[0]->Deactivate();
	}

	const auto firstPipeSpawners = Game::zoneManager->GetSpawnersByName(pipeGroup + "1");

	if (!firstPipeSpawners.empty()) {
		firstPipeSpawners[0]->Activate();
	}
}

void AmDropshipComputer::OnTimerDone(Entity* self, std::string timerName) {
	const auto* const quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (!quickBuildComponent) return;

	if (timerName == "reset" && quickBuildComponent->GetState() == eQuickBuildState::OPEN) {
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}
