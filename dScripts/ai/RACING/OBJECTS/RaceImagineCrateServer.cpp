#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "PossessableComponent.h"
#include "RaceImagineCrateServer.h"
#include "eRacingTaskParam.h"
#include "MissionComponent.h"
#include "SkillComponent.h"
#include "eMissionTaskType.h"

void RaceImagineCrateServer::OnDie(Entity* self, Entity* killer) {
	if (self->GetVar<bool>(u"bIsDead")) {
		return;
	}

	self->SetVar<bool>(u"bIsDead", true);

	if (killer == nullptr) {
		return;
	}

	auto* skillComponent = killer->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	auto* destroyableComponent = killer->GetComponent<DestroyableComponent>();

	if (destroyableComponent != nullptr) {
		destroyableComponent->SetImagination(60);

		Game::entityManager->SerializeEntity(killer);
	}

	// Find possessor of race car to progress missions and update stats.
	auto* possessableComponent = killer->GetComponent<PossessableComponent>();
	if (possessableComponent != nullptr) {

		auto* possessor = Game::entityManager->GetEntity(possessableComponent->GetPossessor());
		if (possessor != nullptr) {

			auto* missionComponent = possessor->GetComponent<MissionComponent>();
			auto* characterComponent = possessor->GetComponent<CharacterComponent>();

			if (characterComponent != nullptr) {
				characterComponent->UpdatePlayerStatistic(RacingImaginationCratesSmashed);
				characterComponent->UpdatePlayerStatistic(RacingSmashablesSmashed);
			}

			// Progress racing smashable missions
			if (missionComponent == nullptr) return;
			missionComponent->Progress(eMissionTaskType::RACING, 0, (LWOOBJID)eRacingTaskParam::SMASHABLES);
		}
	}
}
