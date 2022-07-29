#include "FvBounceOverWall.h"
#include "MissionComponent.h"

void FvBounceOverWall::OnCollisionPhantom(Entity* self, Entity* target) {
	auto missionComponent = target->GetComponent<MissionComponent>();
	if (missionComponent == nullptr) return;

	// We force progress here to the Gate Crasher mission due to an overlap in LOTs with the 'Shark Bite' missions.
	missionComponent->ForceProgress(GateCrasherMissionId, GateCrasherMissionUid, 1);
}
