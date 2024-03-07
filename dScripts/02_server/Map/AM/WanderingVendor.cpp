#include "WanderingVendor.h"
#include "MovementAIComponent.h"
#include "ProximityMonitorComponent.h"

void WanderingVendor::OnStartup(Entity* self) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	// movementAIComponent->Resume();
	self->SetProximityRadius(10, "playermonitor");
}

void WanderingVendor::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status == "ENTER" && entering->IsPlayer()) {
		auto movementAIComponent = self->GetComponent<MovementAIComponent>();
		if (!movementAIComponent) return;
		// movementAIComponent->Pause();
		self->CancelTimer("startWalking");
	} else if (status == "LEAVE") {
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		if (!proximityMonitorComponent) self->AddComponent<ProximityMonitorComponent>();

		const auto proxObjs = proximityMonitorComponent->GetProximityObjects("playermonitor");
		if (proxObjs.empty()) self->AddTimer("startWalking", 1.5);
	}
}

void WanderingVendor::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "startWalking") {
		auto movementAIComponent = self->GetComponent<MovementAIComponent>();
		if (!movementAIComponent) return;
		// movementAIComponent->Resume();
	}
}
