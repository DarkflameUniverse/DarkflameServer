#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "PossessorComponent.h"
#include "RaceImaginePowerup.h"
#include "eRacingTaskParam.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"

void RaceImaginePowerup::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (sender->IsPlayer() && args == "powerup") {
		auto* possessorComponent = sender->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		auto* vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		auto* destroyableComponent = vehicle->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr) {
			return;
		}

		destroyableComponent->Imagine(10);

		auto* missionComponent = sender->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) return;
		missionComponent->Progress(eMissionTaskType::RACING, self->GetLOT(), (LWOOBJID)eRacingTaskParam::COLLECT_IMAGINATION);
	}
}
