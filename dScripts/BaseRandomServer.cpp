#include "BaseRandomServer.h"
#include "dZoneManager.h"
#include "Spawner.h"
#include "dLogger.h"
#include "Entity.h"

void BaseRandomServer::BaseStartup(Entity* self) {
	self->SetVar<std::string>(u"SpawnState", "min");
	self->SetVar<bool>(u"JustChanged", false);

	CheckEvents(self);
	SpawnMapZones(self);
}

void BaseRandomServer::CheckEvents(Entity* self) {
	// TODO: Add events?
}

void BaseRandomServer::SpawnMapZones(Entity* self) {
	for (const auto& pair : sectionMultipliers) {
		const auto sectionName = zonePrefix + "_" + zoneName + "_" + pair.first;

		SpawnSection(self, sectionName, pair.second);
	}

	if (zoneName == "str") {
		SpawnNamedEnemy(self);
	}

	self->SetVar(u"bInit", true);
}

void BaseRandomServer::SpawnSection(Entity* self, const std::string& sectionName, float iMultiplier) {
	Zone* spawnLoad = GetRandomLoad(self, sectionName);

	if (spawnLoad == nullptr) {
		Game::logger->Log("BaseRandomServer", "Failed to find section: %s", sectionName.c_str());

		return;
	}

	for (const auto& spawnerData : spawnLoad->entries) {
		if (spawnerData.name.empty()) {
			continue;
		}

		const auto spawnNum = std::floor(spawnerData.num * iMultiplier);
		const auto spawnerName = sectionName + "_" + spawnerData.name;

		SetSpawnerNetwork(self, spawnerName, spawnNum, spawnerData.lot);
	}
}

void BaseRandomServer::SetSpawnerNetwork(Entity* self, const std::string& spawnerName, int32_t spawnNum, LOT spawnLOT) {
	const auto& spawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName);

	if (spawnLOT == 11217 && spawnNum > 1) {
		spawnNum = 1;
	}

	if (spawners.empty()) {
		Game::logger->Log("BaseRandomServer", "Failed to find spawner: %s", spawnerName.c_str());

		return;
	}

	auto* spawner = spawners[0];

	if (spawnLOT != 0) {
		spawner->SetSpawnLot(spawnLOT);
		spawner->SetRespawnTime(respawnTime);
	}

	if (spawnNum != 0) {
		spawner->SetNumToMaintain(spawnNum);
	}

	if (spawnerName == "Named_Enemies") {
		spawner->SoftReset();
	}

	spawner->Activate();

	if (std::find(spawnersWatched.begin(), spawnersWatched.end(), spawner) != spawnersWatched.end()) {
		return;
	}

	spawner->AddSpawnedEntityDieCallback([this, self, spawner]() {
		NotifySpawnerOfDeath(self, spawner);
		});

	spawnersWatched.push_back(spawner);
}

BaseRandomServer::Zone* BaseRandomServer::GetRandomLoad(Entity* self, const std::string& sectionName) {
	const auto zoneInfo = GeneralUtils::SplitString(sectionName, '_');

	int32_t totalWeight = 0;

	for (const auto& load : zones) {
		totalWeight += load.iChance;
	}

	const auto randWeight = GeneralUtils::GenerateRandomNumber<int32_t>(0, totalWeight);

	int32_t weight = 0;
	for (auto& zone : zones) {
		weight += zone.iChance;

		if (randWeight <= weight) {
			return &zone;
		}
	}

	return nullptr;
}

void BaseRandomServer::NotifySpawnerOfDeath(Entity* self, Spawner* spawner) {
	const auto& spawnerName = spawner->GetName();

	if (spawnerName == "Named_Enemies") {
		NamedEnemyDeath(self, spawner);

		return;
	}

	const auto& sectionName = spawnerName.substr(0, spawnerName.size() - 7);

	const auto variableName = u"mobsDead" + GeneralUtils::ASCIIToUTF16(sectionName);

	auto mobDeathCount = self->GetVar<int32_t>(variableName);

	mobDeathCount++;

	if (mobDeathCount >= mobDeathResetNumber) {
		const auto& zoneInfo = GeneralUtils::SplitString(sectionName, '_');

		SpawnSection(self, sectionName, sectionMultipliers[zoneInfo[sectionIDConst - 1]]);
	}

	self->SetVar(variableName, mobDeathCount);
}

void BaseRandomServer::NamedEnemyDeath(Entity* self, Spawner* spawner) {
	const auto spawnDelay = GeneralUtils::GenerateRandomNumber<float>(1, 2) * 450;

	self->AddTimer("SpawnNewEnemy", spawnDelay);
}

void BaseRandomServer::SpawnersUp(Entity* self) {

}

void BaseRandomServer::SpawnersDown(Entity* self) {

}

void BaseRandomServer::BaseOnTimerDone(Entity* self, const std::string& timerName) {
	NamedTimerDone(self, timerName);
}

void BaseRandomServer::SpawnNamedEnemy(Entity* self) {
	const auto enemy = namedMobs[GeneralUtils::GenerateRandomNumber<int32_t>(0, namedMobs.size() - 1)];

	SetSpawnerNetwork(self, "Named_Enemies", 1, enemy);
}

void BaseRandomServer::NamedTimerDone(Entity* self, const std::string& timerName) {
	if (timerName == "SpawnNewEnemy") {
		SpawnNamedEnemy(self);
	}
}
