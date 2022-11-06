#include "RandomSpawnerZip.h"

void RandomSpawnerZip::OnStartup(Entity* self) {
	zones = {
		{ //-- ** Load 1 -------------------------- **
			{{mobs.stromb, 3, "type1",},
			{mobs.pirate,  2, "type2",},
			{mobs.admiral, 2, "type3",},
			{mobs.spider, 1, "type4",}},
			19
		},
		{ //-- ** Load 2 -------------------------- **
			{{mobs.spider, 1, "type1",},
			{mobs.pirate,  2, "type2",},
			{mobs.pirate, 1, "type3",},
			{mobs.admiral, 1, "type4",}},
			19
		},
		{ //-- ** Load 3 -------------------------- **
			{{mobs.mech, 3, "type1",},
			{mobs.stromb,  1, "type2",},
			{mobs.pirate, 1, "type3",},
			{mobs.stromb, 1, "type4",}},
			10
		},
		{ //-- ** Load 4 -------------------------- **
			{{mobs.horse, 1, "type1",},
			{mobs.stromb,  2, "type2",},
			{mobs.ronin, 1, "type3",},
			{mobs.pirate, 1, "type4",}},
			5
		},
		{ //-- ** Load 5 -------------------------- **
			{{mobs.gorilla, 1, "type1",},
			{mobs.admiral,  1, "type2",},
			{mobs.stromb, 2, "type3",},
			{mobs.pirate, 0, "type4",}},
			1
		},
		{ //-- ** Load 6 -------------------------- **
			{{mobs.ronin, 2, "type1",},
			{mobs.admiral,  2, "type2",},
			{mobs.stromb, 2, "type3",},
			{mobs.mech, 1, "type4",}},
			19
		},
		{ //-- ** Load 7 -------------------------- **
			{{mobs.spider, 2, "type1",},
			{mobs.stromb,  0, "type2",},
			{mobs.ronin, 0, "type3",},
			{mobs.pirate, 0, "type4",}},
			1
		},
		{ //-- ** Load 8 -------------------------- **
			{{mobs.pirate, 4, "type1",},
			{mobs.admiral,  1, "type2",},
			{mobs.ronin, 0, "type3",},
			{mobs.pirate, 0, "type4",}},
			3
		},
		{ //-- ** Load 9 -------------------------- **
			{{mobs.spider, 1, "type1",},
			{mobs.mech,  2, "type2",},
			{mobs.stromb, 2, "type3",},
			{mobs.pirate, 0, "type4",}},
			18
		},
		{ //-- ** Load 10 -------------------------- **
			{{mobs.horse, 1, "type1",},
			{mobs.stromb,  0, "type2",},
			{mobs.ronin, 2, "type3",},
			{mobs.pirate, 0, "type4",}},
			1
		},
	};

	sectionMultipliers = {
		{"secA", 1.2f},
		{"secB", 1.2f},
	};

	zoneName = "zip";
	mobDeathResetNumber = 20;
	changeNum = 9;

	BaseStartup(self);
}

void RandomSpawnerZip::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}
