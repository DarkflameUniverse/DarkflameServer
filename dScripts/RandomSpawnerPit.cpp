#include "RandomSpawnerPit.h"

void RandomSpawnerPit::OnStartup(Entity* self) {
	zones = {
		{ //-- ** Load 1 -------------------------- **
			{{mobs.admiral, 4, "type1",},
			{mobs.spider,  3, "type2",}},
			5
		},
		{ //-- ** Load 2 -------------------------- **
			{{mobs.admiral, 4, "type1",},
			{mobs.pirate,  7, "type2",}},
			15
		},
		{ //-- ** Load 3 -------------------------- **
			{{mobs.spider, 4, "type1",},
			{mobs.stromb,  10, "type2",}},
			15
		},
		{ //-- ** Load 4 -------------------------- **
			{{mobs.mech, 2, "type1",},
			{mobs.horse,  1, "type2",}},
			6
		},
		{ //-- ** Load 5 -------------------------- **
			{{mobs.gorilla, 1, "type1",},
			{mobs.admiral,  4, "type2",}},
			2
		},
		{ //-- ** Load 6 -------------------------- **
			{{mobs.pirate, 7, "type1",},
			{mobs.ronin,  6, "type2",}},
			5
		},
		{ //-- ** Load 7 -------------------------- **
			{{mobs.spider, 3, "type1",},
			{mobs.ronin,  9, "type2",}},
			10
		},
		{ //-- ** Load 8 -------------------------- **
			{{mobs.gorilla, 1, "type1",},
			{mobs.stromb,  8, "type2",}},
			2
		},
		{ //-- ** Load 9 -------------------------- **
			{{mobs.mech, 2, "type1",},
			{mobs.admiral,  4, "type2",}},
			2
		},
		{ //-- ** Load 10 -------------------------- **
			{{mobs.horse, 2, "type1",},
			{mobs.admiral,  3, "type2",}},
			1
		},
		{ //-- ** Load 11 -------------------------- **
			{{mobs.mech, 3, "type1",},
			{mobs.ronin,  5, "type2",}},
			15
		},
		{ //-- ** Load 12 -------------------------- **
			{{mobs.mech, 3, "type1",},
			{mobs.pirate,  5, "type2",}},
			15
		},
	};

	sectionMultipliers = {
		{"secA", 1},
		{"secB", 1.2f},
		{"secC", 1.2f},
		{"secD", 1},
	};

	zoneName = "pit";
	mobDeathResetNumber = 20;
	changeNum = 18;

	BaseStartup(self);
}

void RandomSpawnerPit::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}
