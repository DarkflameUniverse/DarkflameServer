#include "FallingTile.h"
#include "MovingPlatformComponent.h"
#include "GameMessages.h"

void FallingTile::OnStartup(Entity* self) {
	auto* movingPlatfromComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatfromComponent == nullptr) {
		return;
	}

	movingPlatfromComponent->SetSerialized(true);
}

void FallingTile::OnCollisionPhantom(Entity* self, Entity* target) {
	if (self->GetVar<bool>(u"AboutToFall")) {
		return;
	}

	self->AddTimer("flipTime", 0.75f);

	self->SetVar<bool>(u"AboutToFall", true);

	self->SetNetworkVar<float>(u"startEffect", 2);
}

void FallingTile::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	if (waypointIndex == 1) {
	} else if (waypointIndex == 0) {
	}
}

void FallingTile::OnTimerDone(Entity* self, std::string timerName) {
	auto* movingPlatfromComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatfromComponent == nullptr) {
		return;
	}

	if (timerName == "flipTime") {
		self->AddTimer("flipBack", 2.0f);

		self->SetNetworkVar<float>(u"stopEffect", 3);

		movingPlatfromComponent->GotoWaypoint(1);

		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"down", "down");
	} else if (timerName == "flipBack") {
		self->SetVar<bool>(u"AboutToFall", false);

		movingPlatfromComponent->GotoWaypoint(0);

		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"up", "up");
	}
}
