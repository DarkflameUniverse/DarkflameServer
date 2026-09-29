#include "WblStarflies.h"

#include "Entity.h"
#include "EntityManager.h"
#include "eMissionState.h"
#include "MovingPlatformComponent.h"

void WblStarflies::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionState != eMissionState::COMPLETE && missionState != eMissionState::READY_TO_COMPLETE) return;
	for (auto* starfly : Game::entityManager->GetEntitiesInGroup("Starflies")) {
		if (starfly == self) continue;
		if (auto* movingPlatform = starfly->GetComponent<MovingPlatformComponent>()) movingPlatform->StartPathing();
	}
}
