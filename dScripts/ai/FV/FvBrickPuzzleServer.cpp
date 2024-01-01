#include "FvBrickPuzzleServer.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "Spawner.h"
#include "QuickBuildComponent.h"

void FvBrickPuzzleServer::OnStartup(Entity* self) {
	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	int32_t pipeNum = 0;
	if (!GeneralUtils::TryParse<int32_t>(myGroup.substr(10, 1), pipeNum)) {
		return;
	}

	if (pipeNum != 1) {
		self->AddTimer("reset", 30);
	}
}

void FvBrickPuzzleServer::OnDie(Entity* self, Entity* killer) {
	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	int32_t pipeNum = 0;
	if (!GeneralUtils::TryParse<int32_t>(myGroup.substr(10, 1), pipeNum)) {
		return;
	}

	const auto pipeGroup = myGroup.substr(0, 10);

	const auto nextPipeNum = pipeNum + 1;

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

void FvBrickPuzzleServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "reset") {
		auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

		if (quickBuildComponent != nullptr && quickBuildComponent->GetState() == eQuickBuildState::OPEN) {
			self->Smash(self->GetObjectID(), eKillType::SILENT);
		}
	}
}
