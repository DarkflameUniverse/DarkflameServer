#include "CharacterComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"
#include "RaceSmashServer.h"
#include "RacingTaskParam.h"
#include "MissionComponent.h"

void RaceSmashServer::OnDie(Entity* self, Entity* killer) {
	// Crate is smashed by the car
	auto* possessableComponent = killer->GetComponent<PossessableComponent>();
	if (possessableComponent != nullptr) {

		auto* possessor = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());
		if (possessor != nullptr) {

			auto* missionComponent = possessor->GetComponent<MissionComponent>();
			auto* characterComponent = possessor->GetComponent<CharacterComponent>();

			if (characterComponent != nullptr) {
				characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
			}

			// Progress racing smashable missions
			if (missionComponent == nullptr) return;
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_RACING, 0, (LWOOBJID)RacingTaskParam::RACING_TASK_PARAM_SMASHABLES);
			// Progress missions that ask us to smash a specific smashable.
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_RACING, self->GetLOT(), (LWOOBJID)RacingTaskParam::RACING_TASK_PARAM_SMASH_SPECIFIC_SMASHABLE);
		}
	}
}
