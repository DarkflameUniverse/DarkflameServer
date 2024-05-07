#pragma once
#include "CppScripts.h"
#include "FvRacePillarServer.h"

class FvRacePillarDServer : public FvRacePillarServer {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	const LOT m_PillarD = 11949;
	const LOT m_Dragon = 11898;
};
