#pragma once
#include "CppScripts.h"

class FvBrickPuzzleServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
