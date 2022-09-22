#include "RandomSpawnerStr.h"

void RandomSpawnerStr::OnStartup(Entity* self) {
	zones = {
		{ //-- ** Load 1 -------------------------- **
			{{mobs.stromb, 4, "type1",},
			{mobs.pirate,  3, "type2",},
			{mobs.ronin, 3, "type3",}},
			45
		},
		{ //-- ** Load 2 -------------------------- **
			{{mobs.stromb,  3, "type1",},
			{mobs.pirate,    3, "type2",},
			{mobs.mech,     3, "type3",}},
			20
		},
		{ //-- ** Load 3 -------------------------- **
			{{mobs.stromb, 4, "type1",},
			{mobs.admiral, 2, "type2",},
			{mobs.spider, 1, "type3",}},
			10
		},
		{ //-- ** Load 4 -------------------------- **
			{{mobs.mech, 3, "type1",},
			{mobs.spider, 1, "type2",},
			{mobs.stromb, 4, "type3",}},
			3
		},
		{ //-- ** Load 5 -------------------------- **
			{{mobs.horse, 1, "type1",},
			{mobs.ronin, 5, "type2",},
			{mobs.pirate, 2, "type3",}},
			1
		},
		{ //-- ** Load 6 -------------------------- **
			{{mobs.gorilla, 1, "type1",},
			{mobs.pirate, 5, "type2",},
			{mobs.admiral, 2, "type3",}},
			1
		},
		{ //-- ** Load 7 -------------------------- **
			{{mobs.admiral, 2, "type1",},
			{mobs.stromb, 4, "type2",},
			{mobs.ronin, 2, "type3",}},
			3
		},
		{ //-- ** Load 8 -------------------------- **
			{{mobs.admiral, 3, "type1",},
			{mobs.gorilla, 1, "type2",},
			{mobs.horse, 1, "type3",}},
			1
		},
		{ //-- ** Load 9 -------------------------- **
			{{mobs.ronin, 3, "type1",},
			{mobs.ronin, 3, "type2",},
			{mobs.ronin, 3, "type3",}},
			5
		},
		{ //-- ** Load 10 -------------------------- **
			{{mobs.pirate, 4, "type1",},
			{mobs.pirate, 4, "type2",},
			{mobs.pirate, 4, "type3",}},
			1
		},
	};

	sectionMultipliers = {
		{"secA", 1},
		{"secB", 1},
		{"secC", 1.2f},
	};

	zoneName = "str";
	mobDeathResetNumber = 20;
	changeNum = 15;

	BaseStartup(self);
}

void RandomSpawnerStr::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}
