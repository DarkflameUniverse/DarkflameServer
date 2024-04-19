#include "WanderingVendor.h"
#include "MovementAIComponent.h"
#include "ProximityMonitorComponent.h"
#include <ranges>

void WanderingVendor::OnStartup(Entity* self) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	self->SetProximityRadius(10, "playermonitor");
}

void WanderingVendor::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status == "ENTER" && entering->IsPlayer()) {
		auto movementAIComponent = self->GetComponent<MovementAIComponent>();
		if (!movementAIComponent) return;
		movementAIComponent->Pause();
		self->CancelTimer("startWalking");
	} else if (status == "LEAVE") {
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		if (!proximityMonitorComponent) self->AddComponent<ProximityMonitorComponent>();

		const auto proxObjs = proximityMonitorComponent->GetProximityObjects("playermonitor");
		bool foundPlayer = false;
		for (const auto id : proxObjs) {
			auto* entity = Game::entityManager->GetEntity(id);
			if (entity && entity->IsPlayer()) {
				foundPlayer = true;
				break;
			}
		}

		if (!foundPlayer) self->AddTimer("startWalking", 1.5);
	}
}

void WanderingVendor::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "startWalking") {
		auto movementAIComponent = self->GetComponent<MovementAIComponent>();
		if (!movementAIComponent) return;
		movementAIComponent->Resume();
	}
}
