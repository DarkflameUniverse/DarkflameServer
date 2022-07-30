#include "AgStagePlatforms.h"
#include "MovingPlatformComponent.h"

void AgStagePlatforms::OnStartup(Entity* self) {
	auto* component = self->GetComponent<MovingPlatformComponent>();
	if (component) {
		component->SetNoAutoStart(true);
		component->StopPathing();
	}
}

void AgStagePlatforms::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	auto* component = self->GetComponent<MovingPlatformComponent>();
	if (waypointIndex == 0 && component)
		component->StopPathing();
}
