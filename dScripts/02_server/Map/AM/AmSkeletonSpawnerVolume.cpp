#include "AmSkeletonSpawnerVolume.h"

#include "Entity.h"
#include "dZoneManager.h"
#include "Spawner.h"

namespace {
	const std::u16string TotalPlayers = u"TotalPlayers";
	const std::u16string SpawnStateMax = u"SpawnStateMax";
	const std::u16string JustChanged = u"JustChanged";

	void SetNumAndReset(const std::string& spawnerName, const int32_t num) {
		for (auto* const spawner : Game::zoneManager->GetSpawnersByName(spawnerName)) {
			spawner->SetNumToMaintain(num);
			spawner->Reset();
		}
	}
}

void AmSkeletonSpawnerVolume::OnStartup(Entity* self) {
	self->SetVar<int32_t>(TotalPlayers, 0);
	self->SetVar<bool>(SpawnStateMax, false);
	self->SetVar<bool>(JustChanged, false);
}

void AmSkeletonSpawnerVolume::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target || !target->IsPlayer()) return;

	const auto totalPlayers = self->GetVar<int32_t>(TotalPlayers) + 1;
	self->SetVar<int32_t>(TotalPlayers, totalPlayers);

	if (totalPlayers >= ChangeNum && !self->GetVar<bool>(JustChanged) && !self->GetVar<bool>(SpawnStateMax)) {
		SetSpawnCounts(self, true);
	}
}

void AmSkeletonSpawnerVolume::OnOffCollisionPhantom(Entity* self, Entity* target) {
	if (!target || !target->IsPlayer()) return;

	const auto totalPlayers = std::max(self->GetVar<int32_t>(TotalPlayers) - 1, 0);
	self->SetVar<int32_t>(TotalPlayers, totalPlayers);

	if (totalPlayers < ChangeNum && !self->GetVar<bool>(JustChanged) && self->GetVar<bool>(SpawnStateMax)) {
		SetSpawnCounts(self, false);
	}
}

void AmSkeletonSpawnerVolume::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "StateChange") return;

	self->SetVar<bool>(JustChanged, false);

	// Catch up on any change that was held back while the counts were cooling down.
	const auto totalPlayers = self->GetVar<int32_t>(TotalPlayers);
	const auto isMax = self->GetVar<bool>(SpawnStateMax);
	if (!isMax && totalPlayers >= ChangeNum) {
		SetSpawnCounts(self, true);
	} else if (isMax && totalPlayers < ChangeNum) {
		SetSpawnCounts(self, false);
	}
}

void AmSkeletonSpawnerVolume::SetSpawnCounts(Entity* self, const bool max) {
	self->SetVar<bool>(SpawnStateMax, max);
	self->SetVar<bool>(JustChanged, true);
	self->AddTimer("StateChange", ChangeCooldown);

	// There are 6 engineer and 6 miner spawner networks.
	for (int32_t i = 1; i <= 6; i++) {
		SetNumAndReset("Skullkin_Engineer_" + std::to_string(i), max ? 2 : 1);
		SetNumAndReset("Skullkin_Miner_" + std::to_string(i), max ? 6 : 4);
	}

	SetNumAndReset("Skullkin_Patrollers", max ? 6 : 4);
}
