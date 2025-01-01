#include "VehicleDeathTriggerWaterServer.h"

#include "PossessorComponent.h"
#include "RacingControlComponent.h"

void VehicleDeathTriggerWaterServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (target->IsPlayer() && !target->GetIsDead()) {
		const std::vector<Entity*> racingControllers = Game::entityManager->GetEntitiesByComponent(RacingControlComponent::ComponentType);
		for (auto* const racingController : racingControllers) {
			auto* racingControlComponent = racingController->GetComponent<RacingControlComponent>();
			if (racingControlComponent) {
				racingControlComponent->OnRequestDie(target, u"death_water");
			}
		}
	}
}
