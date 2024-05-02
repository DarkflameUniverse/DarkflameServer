#pragma once
#include "CppScripts.h"
#include "FvRacePillarServer.h"

class FvRacePillarABCServer : public FvRacePillarServer {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	const LOT m_PillarA = 11946;
	const LOT m_PillarB = 11947;
	const LOT m_PillarC = 11948;
	const LOT m_Dragon = 11898;
};
