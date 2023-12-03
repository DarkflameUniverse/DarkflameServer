#pragma once
#include "CppScripts.h"

class FvRacePillarABCServer : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	LOT m_PillarA = 11946;
	LOT m_PillarB = 11947;
	LOT m_PillarC = 11949;
	LOT m_Dragon = 11898;
};
