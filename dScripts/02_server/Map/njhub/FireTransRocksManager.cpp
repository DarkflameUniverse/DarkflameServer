#include "FireTransRocksManager.h"

#include <algorithm>

#include "Entity.h"
#include "EntityManager.h"
#include "MovingPlatformComponent.h"

void FireTransRocksManager::OnStartup(Entity* self) {
	self->AddTimer("check", 5.0f);
	if (!self->HasVar(u"RockGroup")) self->SetVar<std::u16string>(u"RockGroup", u"FireTransRocks");
	if (!self->HasVar(u"NumberOfRocks")) self->SetVar<int32_t>(u"NumberOfRocks", 3);
}

void FireTransRocksManager::CheckForRocks(Entity* self) {
	const auto rocks = Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"RockGroup"));
	// Not all there yet: look again in a second
	if (static_cast<int32_t>(rocks.size()) < self->GetVarAs<int32_t>(u"NumberOfRocks")) {
		self->AddTimer("check", 1.0f);
		return;
	}
	self->AddTimer("StartPlatforms", 5.0f);
}

void FireTransRocksManager::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name != "PlatformDown" || !sender) return;
	auto down = self->GetVar<std::vector<LWOOBJID>>(u"PlatTable");
	if (std::ranges::find(down, sender->GetObjectID()) != down.end()) return;
	down.push_back(sender->GetObjectID());
	if (static_cast<int32_t>(down.size()) < self->GetVarAs<int32_t>(u"NumberOfRocks")) {
		self->SetVar<std::vector<LWOOBJID>>(u"PlatTable", down);
		return;
	}
	// All of them are down
	self->AddTimer("restartPlatforms", 1.0f);
}

void FireTransRocksManager::SendRocksUp(Entity* self) {
	for (auto* const rock : Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"RockGroup"))) {
		if (auto* const platform = rock->GetComponent<MovingPlatformComponent>()) platform->GotoWaypoint(1);
	}
}

void FireTransRocksManager::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "check") {
		CheckForRocks(self);
	} else if (timerName == "StartPlatforms") {
		SendRocksUp(self);
	} else if (timerName == "restartPlatforms") {
		self->SetVar<std::vector<LWOOBJID>>(u"PlatTable", {});
		SendRocksUp(self);
	}
}
