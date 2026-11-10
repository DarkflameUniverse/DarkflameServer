#include "SimpleMoverSwitch.h"

#include "Entity.h"
#include "EntityManager.h"
#include "MovingPlatformComponent.h"
#include "SwitchComponent.h"

void SimpleMoverSwitch::OnStartup(Entity* self) {
	RequestSwitches(self);
}

// switchStartup: every switch in the group is asked for its ObjectActivated (and from then on, in turn, its
// ObjectDeactivated and ObjectActivated). A switch loaded after the platform tells its group so in live
// (objectTypeLoaded) and is asked then; here the platform looks again until it has found one.
void SimpleMoverSwitch::RequestSwitches(Entity* self) {
	const auto group = self->GetVarAsString(u"switchGroup");
	if (group.empty()) return;

	bool found = false;
	for (auto* const object : Game::entityManager->GetEntitiesInGroup(group)) {
		if (object == self) continue;
		auto* const switchComponent = object->GetComponent<SwitchComponent>();
		if (!switchComponent) continue;
		found = true;
		switchComponent->AddActivationCallback([platformID = self->GetObjectID()](const bool active) {
			auto* const platformObject = Game::entityManager->GetEntity(platformID);
			if (!platformObject) return;
			// notifyObjectActivated: GoToWaypoint{ iPathIndex = 1 }; notifyObjectDeactivated: GoToWaypoint{ iPathIndex = 0 }
			if (auto* const platform = platformObject->GetComponent<MovingPlatformComponent>()) platform->GotoWaypoint(active ? 1 : 0);
			});
	}
	if (!found) self->AddTimer("FindSwitch", 1.0f);
}

void SimpleMoverSwitch::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FindSwitch") RequestSwitches(self);
}
