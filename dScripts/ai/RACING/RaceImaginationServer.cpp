#include "RaceImaginationServer.h"
#include "dZoneManager.h"

void StartSpawner(const std::vector<Spawner*>& spawner) {
	for (auto* const entity : spawner) {
		entity->Activate();
	}
}

void RaceImaginationServer::OnZoneLoadedInfo(Entity* self, const GameMessages::ZoneLoadedInfo& info) {
	// Spawn imagination pickups
	StartSpawner(Game::zoneManager->GetSpawnersByName("ImaginationSpawn_Min"));
	if (info.maxPlayers > 2) {
		StartSpawner(Game::zoneManager->GetSpawnersByName("ImaginationSpawn_Med"));
	}
	if (info.maxPlayers > 4) {
		StartSpawner(Game::zoneManager->GetSpawnersByName("ImaginationSpawn_Max"));
	}
}
