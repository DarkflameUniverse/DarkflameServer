#pragma once
#include "CppScripts.h"

class FvRacePillarDServer : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	LOT m_PillarD = 11949;
	LOT m_Dragon = 11898;
};
