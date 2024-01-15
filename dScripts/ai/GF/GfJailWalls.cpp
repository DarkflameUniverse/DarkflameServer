#include "GfJailWalls.h"
#include "dZoneManager.h"
#include "GeneralUtils.h"
#include "eQuickBuildState.h"

void GfJailWalls::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("Jail0" + wall)) {
		spawner->Deactivate();
	}

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Deactivate();
	}
}

void GfJailWalls::OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) {
	if (state != eQuickBuildState::RESETTING) return;

	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("Jail0" + wall)) {
		spawner->Activate();
	}

	for (auto* spawner : Game::zoneManager->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Activate();
	}
}
