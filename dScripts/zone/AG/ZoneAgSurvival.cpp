#include "ZoneAgSurvival.h"

Constants ZoneAgSurvival::GetConstants() {
	return Constants{
		.acceptedDelay = 60,
		.startDelay = 2,
		.waveTime = 7,
		.rewardInterval = 5,
		.coolDownTime = 10,
		.mobSet2Wave = 5,
		.mobSet3Wave = 15,
		.unlockNetwork3 = 10,
		.lotPhase = 0,
		.useMobLots = true,
		.baseMobsStartTier = std::vector<uint32_t> {8, 13, 18, 23, 28, 32},
		.randMobsStartTier = std::vector<uint32_t> {2, 10, 15, 20, 25, 30}
	};
}

MobSets ZoneAgSurvival::GetMobSets() {
	return MobSets{
		std::map<std::string, std::vector<LOT>> {
			{"MobA", {6351, 8088, 8089} },
			{"MobB", {6668, 8090, 8091} },
			{"MobC", {6454, 8096, 8097} },
		},
		std::map<std::string, std::vector<std::vector<std::vector<uint32_t>>>> {
			{ BaseMobSet, {
				{ {3, 0, 0}, },
				{ {2, 1, 0}, },
				{ {4, 1, 0}, },
				{ {1, 2, 0}, },
				{ {0, 1, 1}, },
				{ {0, 2, 2}, }
			}},
			{ RandMobSet, {
				{ {4, 0, 0}, {4, 0, 0}, {4, 0, 0}, {4, 0, 0}, {3, 1, 0} },
				{ {4, 1, 0}, {4, 1, 0}, {4, 1, 0}, {4, 1, 0}, {2, 1, 1} },
				{ {1, 2, 0}, {1, 2, 0}, {1, 2, 0}, {1, 2, 0}, {0, 1, 1} },
				{ {1, 2, 1}, {1, 2, 1}, {1, 2, 1}, {0, 2, 1}, {0, 2, 2} },
				{ {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 3}, {0, 1, 3} },
				{ {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3} },
			}}
		}
	};
}

SpawnerNetworks ZoneAgSurvival::GetSpawnerNetworks() {
	return SpawnerNetworks{
		SpawnerNetworkCollection {
			.mobSetName = BaseMobSet,
			.networks = {
				SpawnerNetwork {
					.names = std::vector<std::string> { "Base_MobA", "Base_MobB", "Base_MobC" },
					.number = "",
					.isLocked = false,
					.isActive = false
				},
			}
		},
		SpawnerNetworkCollection {
			.mobSetName = RandMobSet,
			.networks = {
				SpawnerNetwork {
					.names = std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					.number = "01",
					.isLocked = false,
					.isActive = false
				},
				SpawnerNetwork {
					.names = std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					.number = "02",
					.isLocked = false,
					.isActive = false
				},
				SpawnerNetwork {
					.names = std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					.number = "03",
					.isLocked = true,
					.isActive = false
				},
			}
		},
		SpawnerNetworkCollection {
			.mobSetName = "",
			.networks = {
				SpawnerNetwork {
					.names = std::vector<std::string> { "Rewards_" },
					.number = "01",
					.isLocked = false,
					.isActive = false
				},
			}
		},
		SpawnerNetworkCollection {
			.mobSetName = "",
			.networks = {
				SpawnerNetwork {
					.names = std::vector<std::string> { "Smash_" },
					.number = "01",
					.isLocked = false,
					.isActive = false
				},
			}
		}
	};
}

std::map<uint32_t, uint32_t> ZoneAgSurvival::GetMissionsToUpdate() {
	// Mission : time to complete mission
	return std::map<uint32_t, uint32_t> {
		{ 479, 60 },
		{ 1153, 180 },
		{ 1618, 420 },
		{ 1628, 420 },
		{ 1638, 420 },
		{ 1648, 420 },
		{ 1412, 120 },
		{ 1510, 120 },
		{ 1547, 120 },
		{ 1584, 120 },
		{ 1426, 300 },
		{ 1524, 300 },
		{ 1561, 300 },
		{ 1598, 300 },
		{ 1865, 180 }
	};
}
