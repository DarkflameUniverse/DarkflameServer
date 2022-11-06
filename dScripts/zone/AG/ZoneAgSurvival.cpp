#include "ZoneAgSurvival.h"

Constants ZoneAgSurvival::GetConstants() {
	return Constants{
		60,
		2,
		7,
		5,
		10,
		5,
		15,
		10,
		0,
		true,
		std::vector<uint32_t> {8, 13, 18, 23, 28, 32},
		std::vector<uint32_t> {2, 10, 15, 20, 25, 30}
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
			BaseMobSet,
			{
				SpawnerNetwork {
					std::vector<std::string> { "Base_MobA", "Base_MobB", "Base_MobC" },
					"",
					false,
					false
				},
			}
		},
		SpawnerNetworkCollection {
			RandMobSet,
			{
				SpawnerNetwork {
					std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					"01",
					false,
					false
				},
				SpawnerNetwork {
					std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					"02",
					false,
					false
				},
				SpawnerNetwork {
					std::vector<std::string> {"MobA_", "MobB_", "MobC_"},
					"03",
					true,
					false
				},
			}
		},
		SpawnerNetworkCollection {
			"",
			{
				SpawnerNetwork {
					std::vector<std::string> { "Rewards_" },
					"01",
					false,
					false
				},
			}
		},
		SpawnerNetworkCollection {
			"",
			{
				SpawnerNetwork {
					std::vector<std::string> { "Smash_" },
					"01",
					false,
					false
				},
			}
		}
	};
}

std::map<uint32_t, uint32_t> ZoneAgSurvival::GetMissionsToUpdate() {
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
