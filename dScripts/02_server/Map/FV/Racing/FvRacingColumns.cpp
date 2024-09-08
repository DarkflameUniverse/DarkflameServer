#include "FvRacingColumns.h"
#include "MovingPlatformComponent.h"

void FvRacingColumns::OnStartup(Entity* self) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (!movingPlatformComponent) return;

	movingPlatformComponent->StopPathing();
	movingPlatformComponent->SetSerialized(true);
	int32_t pathStart = 0;
	if (self->HasVar(u"attached_path_start")) {
		pathStart = self->GetVar<uint32_t>(u"attached_path_start");
	}
	movingPlatformComponent->WarpToWaypoint(pathStart);
}
