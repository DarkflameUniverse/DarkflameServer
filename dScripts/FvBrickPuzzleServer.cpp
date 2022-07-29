#include "FvBrickPuzzleServer.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "Spawner.h"
#include "RebuildComponent.h"

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

void FvBrickPuzzleServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "reset") {
		auto* rebuildComponent = self->GetComponent<RebuildComponent>();

		if (rebuildComponent != nullptr && rebuildComponent->GetState() == REBUILD_OPEN) {
			self->Smash(self->GetObjectID(), SILENT);
		}
	}
}
