#pragma once
#include "CppScripts.h"

class AgShipShake : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
	void DoShake(Entity* self, bool explodeIdle);

	std::string DebrisFX = "DebrisFX";
	std::string ShipFX = "ShipFX";
	std::string ShipFX2 = "ShipFX2";
	std::u16string FXName = u"camshake-bridge";

private:
	Entity* GetEntityInGroup(const std::string& group);
};

