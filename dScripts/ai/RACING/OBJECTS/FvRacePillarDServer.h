#pragma once
#include "CppScripts.h"
#include "FvRacePillarServer.h"

class FvRacePillarDServer : public FvRacePillarServer {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	LOT m_PillarD = 11949;
	LOT m_Dragon = 11898;
};
