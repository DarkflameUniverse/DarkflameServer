#include "FvRaceSmashEggImagineServer.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"
#include "eRacingTaskParam.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"

void FvRaceSmashEggImagineServer::OnDie(Entity* self, Entity* killer) {
	if (killer != nullptr) {
		auto* destroyableComponent = killer->GetComponent<DestroyableComponent>();
		if (destroyableComponent != nullptr) {
			destroyableComponent->SetImagination(destroyableComponent->GetImagination() + 10);
			Game::entityManager->SerializeEntity(killer);
		}

		// get possessor to progress statistics and tasks.
		auto* possessableComponent = killer->GetComponent<PossessableComponent>();
		if (possessableComponent != nullptr) {

			auto* possessor = Game::entityManager->GetEntity(possessableComponent->GetPossessor());
			if (possessor != nullptr) {

				auto* missionComponent = possessor->GetComponent<MissionComponent>();
				auto* characterComponent = possessor->GetComponent<CharacterComponent>();
				if (characterComponent != nullptr) {
					characterComponent->UpdatePlayerStatistic(ImaginationPowerUpsCollected);
					characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
				}
				if (missionComponent == nullptr) return;
				// Dragon eggs have their own smash server so we handle mission progression for them here.
				missionComponent->Progress(eMissionTaskType::RACING, 0, (LWOOBJID)eRacingTaskParam::SMASHABLES);
				missionComponent->Progress(eMissionTaskType::RACING, self->GetLOT(), (LWOOBJID)eRacingTaskParam::SMASH_SPECIFIC_SMASHABLE);
			}
		}

	}
}
