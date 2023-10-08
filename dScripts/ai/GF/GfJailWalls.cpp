#include "GfJailWalls.h"
#include "dZoneManager.h"
#include "GeneralUtils.h"
#include "eRebuildState.h"

void GfJailWalls::OnRebuildComplete(Entity* self, Entity* target) {
	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("Jail0" + wall)) {
		spawner->Deactivate();
	}

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Deactivate();
	}
}

void GfJailWalls::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state != eRebuildState::RESETTING) return;

	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("Jail0" + wall)) {
		spawner->Activate();
	}

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Activate();
	}
}
