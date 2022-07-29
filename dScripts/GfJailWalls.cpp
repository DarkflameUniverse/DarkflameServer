#include "GfJailWalls.h"
#include "dZoneManager.h"
#include "GeneralUtils.h"

void GfJailWalls::OnRebuildComplete(Entity* self, Entity* target) {
	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName("Jail0" + wall)) {
		spawner->Deactivate();
	}

	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Deactivate();
	}
}

void GfJailWalls::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state != eRebuildState::REBUILD_RESETTING) return;

	const auto wall = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Wall"));

	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName("Jail0" + wall)) {
		spawner->Activate();
	}

	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName("JailCaptain0" + wall)) {
		spawner->Activate();
	}
}
