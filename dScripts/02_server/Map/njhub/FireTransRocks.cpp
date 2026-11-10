#include "FireTransRocks.h"

#include "Entity.h"
#include "EntityManager.h"
#include "MovingPlatformComponent.h"

void FireTransRocks::OnStartup(Entity* self) {
	if (!self->HasVar(u"RockManagerGroup")) self->SetVar<std::u16string>(u"RockManagerGroup", u"FireTransRocksManager");
}

void FireTransRocks::OnArrivedAtDesiredWaypoint(Entity* self, const uint32_t waypointIndex) {
	if (waypointIndex == 1) {
		self->AddTimer("PlatHold", 3.5f);
		return;
	}
	// The first manager there is hears that the rock is down
	const auto managers = Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"RockManagerGroup"));
	if (!managers.empty()) managers.front()->GetScript()->OnNotifyObject(managers.front(), self, "PlatformDown");
}

void FireTransRocks::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "PlatHold") return;
	if (auto* const platform = self->GetComponent<MovingPlatformComponent>()) platform->GotoWaypoint(0);
}
