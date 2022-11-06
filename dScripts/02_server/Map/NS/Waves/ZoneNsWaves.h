#pragma once
#include "BaseWavesServer.h"

enum SpawnerName {
	interior_A,
	interior_B,
	interior_C,
	gf_A,
	gf_B,
	gf_C,
	concert_A,
	concert_B,
	concert_C,
	ag_A,
	ag_B,
	ag_C,
	Reward_01,
	interior_Reward,
	Obstacle,
	Boss,
	Ape_Boss,
	Geyser,
	Treasure_01,
	Cavalry_Boss,
	Horseman_01,
	Horseman_02,
	Horseman_03,
	Horseman_04,
};

enum SpawnLOTS : LOT {
	stromling = 12586,
	mech = 12587,
	spiderling = 12588,
	pirate = 12589,
	admiral = 12590,
	ape_boss = 12591,
	stromling_boss = 12600,
	hammerling = 12602,
	sentry = 12604,
	spiderling_ve = 12605,
	spiderling_boss = 12609,
	ronin = 12610,
	cavalry = 12611,
	dragon_boss = 12612,
	stromling_minifig = 12586,
	mushroom = 12614,
	maelstrom_chest = 4894,
	outhouse = 12616,
	dragon_statue = 12617,
	treasure_chest = 12423,
	hammerling_melee = 12653,
	maelstrom_geyser = 10314,
	ronin_statue = 12611,
	horseman_boss01 = 11999,
	horseman_boss02 = 12467,
	horseman_boss03 = 12468,
	horseman_boss04 = 12469,
	admiral_cp = 13523,
};

class ZoneNsWaves : public BaseWavesServer {
	WaveConstants GetConstants() override;
	std::vector<std::string> GetSpawnerNames() override;
	std::vector<WaveMission> GetWaveMissions() override;
	std::vector<Wave> GetWaves() override;
private:
	static std::string GetSpawnerName(SpawnerName spawnerName);
};
