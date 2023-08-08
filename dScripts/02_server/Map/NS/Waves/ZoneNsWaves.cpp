#include "ZoneNsWaves.h"

WaveConstants ZoneNsWaves::GetConstants() {
	return WaveConstants{
		.acceptedDelay = 60,
		.startDelay = 2,
		.waveTime = 6,
		.waveCompleteDelay = 2,
		.eventGroup = "surprise",
		.introCelebration = "intro"
	};
}

std::vector<std::string> ZoneNsWaves::GetSpawnerNames() {
	return {
		"Base_MobA",
		"Base_MobB",
		"Base_MobC",
		"MobA_01",
		"MobB_01",
		"MobC_01",
		"MobA_02",
		"MobB_02",
		"MobC_02",
		"MobA_03",
		"MobB_03",
		"MobC_03",
		"Reward_01",
		"Base_Reward",
		"Obstacle_01",
		"Boss",
		"Ape_Boss",
		"Geyser_01",
		"Treasure_01",
		"Cavalry_Boss",
		"Horseman_01",
		"Horseman_02",
		"Horseman_03",
		"Horseman_04"
	};
}

std::vector<WaveMission> ZoneNsWaves::GetWaveMissions() {
	return {
		WaveMission{
			.time = 190,
			.wave = 7,
			.missionID = 1242
		},
		WaveMission{
			.time = 240,
			.wave = 7,
			.missionID = 1226
		},
		WaveMission{
			.time = 450,
			.wave = 15,
			.missionID = 1243
		},
		WaveMission{
			.time = 600,
			.wave = 15,
			.missionID = 1227
		},
		WaveMission{
			.time = 720,
			.wave = 22,
			.missionID = 1244
		},
		WaveMission{
			.time = 840,
			.wave = 22,
			.missionID = 1228
		},
		WaveMission{
			.time = 1080,
			.wave = 29,
			.missionID = 1245
		},
		WaveMission{
			.time = 1200,
			.wave = 29,
			.missionID = 1229
		},
	};
}

std::vector<Wave> ZoneNsWaves::GetWaves() {
	return std::vector<Wave>{
		// Wave 1
		Wave{
			std::vector<MobDefinition> {
				{ .lot = SpawnLOTS::stromling_minifig, .amountToSpawn = 8, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
				{ .lot = SpawnLOTS::stromling_minifig, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
				{ .lot = SpawnLOTS::stromling_minifig, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
				{ .lot = SpawnLOTS::stromling_minifig, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
			}
		},

			// Wave 2
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 8, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
				}
		},

			// Wave 3
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
				},
		},

			// Wave 4
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 5
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 6
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::stromling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 7
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::stromling_boss, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Boss) },
				},
				.soloMissions = {1885},
				.missions = {},
				.cinematic = "Stromling_Boss",
				.cinematicLength = 5.0f
		},

			// Wave 8
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{.lot = SpawnLOTS::mushroom, .amountToSpawn = 6, .spawnerName = GetSpawnerName(SpawnerName::Reward_01) },
					{.lot = SpawnLOTS::mushroom, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_Reward) },
				},
				.soloMissions = {},
				.missions = {},
				.cinematic = "",
				.cinematicLength = -1.0f,
				.timeLimit = 25,
		},

			// Wave 9
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 10
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 11
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
				}
		},

			// Wave 12
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
				}
		},

			// Wave 13
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 14
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::mech, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
				}
		},

			// Wave 15
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::ape_boss, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Ape_Boss) },

				},
				.soloMissions = {1886},
				.missions = {},
				.cinematic = "Gorilla_Boss",
				.cinematicLength = 5.0f
		},

			// Wave 16
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{.lot = SpawnLOTS::outhouse, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_Reward) },
					{.lot = SpawnLOTS::mushroom, .amountToSpawn = 6, .spawnerName = GetSpawnerName(SpawnerName::Reward_01) },
				},
				.soloMissions = {},
				.missions = {},
				.cinematic = "",
				.cinematicLength = -1.0f,
				.timeLimit = 25,
		},

			// Wave 17
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 18
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::hammerling_melee, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 19
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
				}
		},

			// Wave 20
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::hammerling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::sentry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
				}
		},

			// Wave 21
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_C) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::admiral, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
				}
		},

			// Wave 22
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::spiderling_boss, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Cavalry_Boss) },
				},
				.soloMissions = {1887},
				.missions = {},
				.cinematic = "Spiderling_Boss",
				.cinematicLength = 5.0f
		},

			// Wave 23
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::outhouse, .amountToSpawn = 6, .spawnerName = GetSpawnerName(SpawnerName::Reward_01) },
					{ .lot = SpawnLOTS::outhouse, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_Reward) },
					{ .lot = SpawnLOTS::maelstrom_chest, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::Obstacle) },
				},
				.soloMissions = {},
				.missions = {},
				.cinematic = "",
				.cinematicLength = -1.0f,
				.timeLimit = 25,
		},

			// Wave 24
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::pirate, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
				}
		},

			// Wave 25
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::cavalry, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::cavalry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::concert_A) },
					{ .lot = SpawnLOTS::spiderling, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_A) },
				}
		},

			// Wave 26
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 3, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::spiderling_ve, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 2, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
				}
		},

			// Wave 27
			Wave{
				std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 5, .spawnerName = GetSpawnerName(SpawnerName::interior_A) },
					{ .lot = SpawnLOTS::ronin, .amountToSpawn = 4, .spawnerName = GetSpawnerName(SpawnerName::interior_B) },
					{ .lot = SpawnLOTS::cavalry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_C) },
					{ .lot = SpawnLOTS::cavalry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_C) },
					{ .lot = SpawnLOTS::cavalry, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_C) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::ag_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::gf_B) },
					{ .lot = SpawnLOTS::admiral_cp, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::concert_B) },
				}
		},

			// Wave 28
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::dragon_statue, .amountToSpawn = 12, .spawnerName = GetSpawnerName(SpawnerName::Reward_01) },
				},
				.soloMissions = {},
				.missions = {},
				.cinematic = "",
				.cinematicLength = -1.0f,
				.timeLimit = 30,
		},

			// Wave 29
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::horseman_boss01, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Horseman_01) },
					{ .lot = SpawnLOTS::horseman_boss02, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Horseman_02) },
					{ .lot = SpawnLOTS::horseman_boss03, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Horseman_03) },
					{ .lot = SpawnLOTS::horseman_boss04, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Horseman_04) },
				},
				.soloMissions = {1888},
				.missions = {1236, 1237, 1249},
				.cinematic = "Horsemen_Boss",
				.cinematicLength = 5.0f
		},

			// Wave 30 (treasure)
			Wave{
				.waveMobs = std::vector<MobDefinition> {
					{ .lot = SpawnLOTS::treasure_chest, .amountToSpawn = 1, .spawnerName = GetSpawnerName(SpawnerName::Treasure_01) },
				},
				.soloMissions = {},
				.missions = {},
				.cinematic = "Treasure_Camera",
				.cinematicLength = 5.0f,
				.timeLimit = (uint32_t)-1,
				.notifyWin = true,
				.winDelay = 60,
		},
	};
}

std::string ZoneNsWaves::GetSpawnerName(SpawnerName spawnerName) {
	switch (spawnerName) {
	case interior_A:
		return "Base_MobA";
	case interior_B:
		return "Base_MobB";
	case interior_C:
		return "Base_MobC";
	case gf_A:
		return "MobA_01";
	case gf_B:
		return "MobB_01";
	case gf_C:
		return "MobC_01";
	case concert_A:
		return "MobA_02";
	case concert_B:
		return "MobB_02";
	case concert_C:
		return "MobC_02";
	case ag_A:
		return "MobA_03";
	case ag_B:
		return "MobB_03";
	case ag_C:
		return "MobC_03";
	case Reward_01:
		return "Reward_01";
	case interior_Reward:
		return "Base_Reward";
	case Obstacle:
		return "Obstacle_01";
	case Boss:
		return "Boss";
	case Ape_Boss:
		return "Ape_Boss";
	case Geyser:
		return "Geyser_01";
	case Treasure_01:
		return "Treasure_01";
	case Cavalry_Boss:
		return "Cavalry_Boss";
	case Horseman_01:
		return "Horseman_01";
	case Horseman_02:
		return "Horseman_02";
	case Horseman_03:
		return "Horseman_03";
	case Horseman_04:
		return "Horseman_04";
	default:
		return "";
	}
}
