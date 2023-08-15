#include "CharacterComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"
#include "RaceSmashServer.h"
#include "eRacingTaskParam.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"

void RaceSmashServer::OnDie(Entity* self, Entity* killer) {
	// Crate is smashed by the car
	auto* possessableComponent = killer->GetComponent<PossessableComponent>();
	if (possessableComponent != nullptr) {

		auto* possessor = Game::entityManager->GetEntity(possessableComponent->GetPossessor());
		if (possessor != nullptr) {

			auto* missionComponent = possessor->GetComponent<MissionComponent>();
			auto* characterComponent = possessor->GetComponent<CharacterComponent>();

			if (characterComponent != nullptr) {
				characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
			}

			// Progress racing smashable missions
			if (missionComponent == nullptr) return;
			missionComponent->Progress(eMissionTaskType::RACING, 0, (LWOOBJID)eRacingTaskParam::SMASHABLES);
			// Progress missions that ask us to smash a specific smashable.
			missionComponent->Progress(eMissionTaskType::RACING, self->GetLOT(), (LWOOBJID)eRacingTaskParam::SMASH_SPECIFIC_SMASHABLE);
		}
	}
}
