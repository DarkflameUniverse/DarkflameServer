#include "RandomSpawnerFin.h"

void RandomSpawnerFin::OnStartup(Entity* self) {
	zones = {
		{ //-- ** Load 1 -------------------------- **
			{{mobs.pirate, 3, "type1",},
			{mobs.ronin,  3, "type2",},
			{mobs.spider, 2, "type3",}},
			10
		},
		{ //-- ** Load 2 -------------------------- **
			{{mobs.admiral,  3, "type1",},
			{mobs.ronin,    2, "type2",},
			{mobs.mech,     2, "type3",}},
			5
		},
		{ //-- ** Load 3 -------------------------- **
			{{mobs.horse, 2, "type1",},
			{mobs.admiral, 3, "type2",},
			{mobs.stromb, 5, "type3",}},
			10
		},
		{ //-- ** Load 4 -------------------------- **
			{{mobs.horse, 1, "type1",},
			{mobs.gorilla, 1, "type2",},
			{mobs.pirate, 4, "type3",}},
			2
		},
		{ //-- ** Load 5 -------------------------- **
			{{mobs.spider, 1, "type1",},
			{mobs.mech, 2, "type2",},
			{mobs.gorilla, 1, "type3",}},
			1
		},
		{ //-- ** Load 6 -------------------------- **
			{{mobs.mech, 2, "type1",},
			{mobs.pirate, 4, "type2",},
			{mobs.horse, 1, "type3",}},
			10
		},
		{ //-- ** Load 7 -------------------------- **
			{{mobs.stromb, 3, "type1",},
			{mobs.spider, 1, "type2",},
			{mobs.horse, 1, "type3",}},
			5
		},
		{ //-- ** Load 8 -------------------------- **
			{{mobs.pirate, 3, "type1",},
			{mobs.admiral, 2, "type2",},
			{mobs.gorilla, 1, "type3",}},
			2
		},
		{ //-- ** Load 9 -------------------------- **
			{{mobs.stromb, 3, "type1",},
			{mobs.mech, 2, "type2",},
			{mobs.spider, 1, "type3",}},
			10
		},
		{ //-- ** Load 10 -------------------------- **
			{{mobs.admiral, 3, "type1",},
			{mobs.pirate, 3, "type2",},
			{mobs.horse, 1, "type3",}},
			10
		},
	};

	sectionMultipliers = {
		{"secA", 1},
		{"secB", 1},
		{"secC", 1.2f},
		{"secD", 1.3f},
		{"secE", 1.6f},
		{"secF", 1},
		{"secG", 1},
		{"secH", 1.2f},
	};

	zoneName = "fin";

	BaseStartup(self);
}

void RandomSpawnerFin::OnTimerDone(Entity* self, std::string timerName) {
	BaseOnTimerDone(self, timerName);
}
