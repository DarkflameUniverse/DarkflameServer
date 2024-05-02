#pragma once
#include "CppScripts.h"

class FvRaceDragonLap1 : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	LOT m_Dragon = 11898;
};
