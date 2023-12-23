#pragma once
#include "CppScripts.h"

class WildSnake05 : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnDie(Entity* self, Entity* killer) override;
};
