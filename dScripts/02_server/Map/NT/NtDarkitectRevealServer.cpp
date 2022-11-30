#include "NtDarkitectRevealServer.h"
#include "Darkitect.h"
#include "MissionComponent.h"

void NtDarkitectRevealServer::OnUse(Entity* self, Entity* user) {
	Darkitect Baron;
	Baron.Reveal(self, user);

	auto* missionComponent = user->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->ForceProgressTaskType(1344, 1, 14293);
	}
}
