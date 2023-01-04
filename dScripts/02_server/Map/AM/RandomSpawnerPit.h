#pragma once
#include "CppScripts.h"
#include "BaseRandomServer.h"

class RandomSpawnerPit : public CppScripts::Script, BaseRandomServer
{
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	struct Mobs
	{
		static const LOT stromb = 11212;
		static const LOT mech = 11213;
		static const LOT spider = 11214;
		static const LOT pirate = 11215;
		static const LOT admiral = 11216;
		static const LOT gorilla = 11217;
		static const LOT ronin = 11218;
		static const LOT horse = 11219;
		static const LOT dragon = 112200;
	};

	Mobs mobs;
};

