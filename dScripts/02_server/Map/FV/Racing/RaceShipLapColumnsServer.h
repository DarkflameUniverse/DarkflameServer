#pragma once
#include "CppScripts.h"

class RaceShipLapColumnsServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
